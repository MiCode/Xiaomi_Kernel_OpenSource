// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/thermal.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE)
/* TODO: wait for cpufreq_lite */
/* extern int get_devinfo(int i); */
#endif

#if IS_ENABLED(CONFIG_MTK_SWPM_PERF_ARMV8_PMU)
#include <swpm_perf_arm_pmu.h>
#endif

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL)
#include <thermal_interface.h>
#endif

#define CREATE_TRACE_POINTS
#define DRAM_LP5 (0)
#include <swpm_tracker_trace.h>
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power); */
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power_idx); */

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6855.h>
#include <swpm_v6855_ext.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
/* index snapshot */
DEFINE_SPINLOCK(swpm_snap_spinlock);
struct mem_swpm_index mem_idx_snap;
struct power_rail_data swpm_power_rail[NR_POWER_RAIL] = {
	[VPROC2] = {0, "VPROC2"},
	[VPROC1] = {0, "VPROC1"},
	[VGPU] = {0, "VGPU"},
	[VCORE] = {0, "VCORE"},
	[VDRAM] = {0, "VDRAM"},
};
struct share_wrap *wrap_d;

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static unsigned int swpm_init_state;
static void swpm_init_retry(struct work_struct *work);
static struct workqueue_struct *swpm_init_retry_work_queue;
DECLARE_WORK(swpm_init_retry_work, swpm_init_retry);

static unsigned char avg_window = DEFAULT_AVG_WINDOW;
static unsigned int swpm_log_mask = DEFAULT_LOG_MASK;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
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

struct swpm_rec_data *swpm_info_ref;
struct cpu_swpm_rec_data *cpu_ptr;
struct core_swpm_rec_data *core_ptr;
struct mem_swpm_rec_data *mem_ptr;
struct me_swpm_rec_data *me_ptr;

/* rt => /100000, uA => *1000, res => 100 */
#define CORE_DEFAULT_DEG (30)
#define CORE_STATIC_MA (70)
#define CORE_STATIC_ID (7)
#define CORE_STATIC_RT_RES (100)
#define V_OF_CORE_STATIC (750)
static char core_ts_name[NR_CORE_TS][MAX_POWER_NAME_LENGTH] = {
	"soc1", "soc2", "soc3", "soc4",
};
static unsigned int core_static;
static unsigned int core_static_replaced;
static unsigned short core_static_rt[NR_CORE_STATIC_TYPE] = {
	2170, 3804, 3931, 4335, 10544, 11673, 11788, 13976, 17134, 20646
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	550, 600, 650, 750,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1066, 1600, 2133, 2750,
};
static unsigned short dram_pwr_sample[NR_DRAM_PWR_SAMPLE] = {
#if DRAM_LP5
	/*Support for LP5*/
	800, 1333, 2750,
#else
	/*Support for LP4*/
	1600, 1600, 1600,
#endif
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
	[DDR_1066] = {
	.bw = {	80, 197, 716, 1800, 3432, 3998,
	4492, 5491, 5930, 6414, 6990, 7864},
	},
	[DDR_1600] = {
	.bw = {	118, 292, 1109, 2765, 5270, 6116,
	6835, 8339, 8973, 9667, 10530, 12019},
	},
	[DDR_2133] = {
	.bw = {	160, 393, 1479, 3708, 6997, 8053,
	8978, 10943, 11692, 12931, 13839, 15679},
	},
	[DDR_2750] = {
	.bw = {	207, 506, 1906, 4780, 9021, 10382,
	11575, 14108, 15073, 16671, 17842, 20214},
	},
};
static struct aphy_core_pwr_data aphy_def_core_pwr_tbl[] = {
	[APHY_VCORE] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {4, 9, 18, 34, 71, 104,
	135, 168, 188, 203, 204, 208},
			.write_coef = {7, 16, 32, 61, 135, 196,
	256, 322, 382, 407, 407, 407},
		},
		[DDR_600] = {
			.read_coef = {5, 12, 44, 104, 188, 225,
	237, 246, 248, 249, 250, 250},
			.write_coef = {12, 28, 106, 259, 498, 545,
	608, 608, 608, 608, 608, 610},
		},
		[DDR_800] = {
			.read_coef = {42, 106, 300, 485, 687, 745,
	788, 788, 789, 790, 790, 790},
			.write_coef = {49, 111, 361, 636, 1005, 1096,
	1135, 1136, 1136, 1136, 1137, 1139},
		},
		[DDR_933] = {
			.read_coef = {55, 141, 393, 634, 884, 960,
	1009, 1013, 1013, 1013, 1013, 1013},
			.write_coef = {58, 148, 456, 802, 1266, 1381,
	1478, 1480, 1482, 1482, 1482, 1485},
		},
		[DDR_1066] = {
			.read_coef = {62, 158, 445, 719, 1001, 1087,
	1143, 1147, 1147, 1147, 1147, 1147},
			.write_coef = {66, 168, 517, 909, 1433, 1564,
	1674, 1676, 1679, 1679, 1679, 1682},
		},
		[DDR_1600] = {
			.read_coef = {140, 351, 910, 1392, 1865, 2019,
	2085, 2086, 2087, 2088, 2095, 2116},
			.write_coef = {143, 342, 1009, 1717, 2570, 2804,
	2994, 2999, 3003, 3003, 3003, 3045},
		},
		[DDR_2133] = {
			.read_coef = {275, 600, 1552, 2396, 3010, 3211,
	3307, 3367, 3376, 3385, 3385, 3385},
			.write_coef = {285, 590, 1698, 2981, 4147, 4500,
	4819, 4846, 4846, 4871, 4871, 4871},
		},
		[DDR_2750] = {
			.read_coef = {429, 931, 2414, 3726, 4682, 4995,
	5144, 5237, 5252, 5265, 5265, 5265},
			.write_coef = {443, 918, 2641, 4636, 6451, 7000,
	7496, 7538, 7538, 7577, 7577, 7577},
		},
	},
	.coef_idle = {498, 586, 674, 813, 961, 1265, 2230, 3006},
	.coef_srst = {70, 70, 70, 78, 84, 119, 163, 204},
	.coef_ssr = {14, 14, 14, 16, 17, 21, 27, 28},
	.volt = {550, 550, 550, 550, 550, 600, 650, 725},
	},
};
static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
	.bw = {	32, 64, 160, 320, 640,
	960, 1280, 1600, 1920, 2240, 2560},
	},
	[DDR_600] = {
	.bw = {	48, 96, 240, 480, 960,
	1440, 1920, 2400, 2880, 3360, 3840},
	},
	[DDR_800] = {
	.bw = {	64, 128, 320, 640, 1280,
	1920, 2560, 3200, 3840, 4480, 5120},
	},
	[DDR_933] = {
	.bw = {	75, 149, 373, 746, 1493,
	2239, 2986, 3732, 4478, 5225, 5971},
	},
	[DDR_1066] = {
	.bw = {	85, 171, 427, 853, 1706,
	2560, 3413, 4266, 5119, 5972, 6826},
	},
	[DDR_1600] = {
	.bw = {	128, 256, 640, 1280, 2560,
	3840, 5120, 6400, 7680, 8960, 10240},
	},
	[DDR_2133] = {
	.bw = {	171, 341, 853, 1706, 3413,
	5119, 6826, 8532, 10238, 11945, 13651},
	},
	[DDR_2750] = {
	.bw = {	220, 440, 1100, 2200, 4400,
	6600, 8800, 11000, 13200, 15400, 17600},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {
	[APHY_VDDQ] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {0, 0, 89, 158, 255,
	340, 445, 484, 509, 576, 644},
			.write_coef = {55, 82, 201, 404, 752,
	996, 1252, 1494, 1723, 1949, 2180},
		},
		[DDR_600] = {
			.read_coef = {38, 71, 149, 233, 474,
	620, 556, 630, 762, 863, 931},
			.write_coef = {86, 152, 380, 642, 1243,
	1490, 1812, 2288, 2664, 3016, 3256},
		},
		[DDR_800] = {
			.read_coef = {35, 82, 165, 314, 571,
	654, 787, 825, 879, 1002, 1075},
			.write_coef = {97, 189, 413, 723, 1351,
	1781, 2323, 2734, 3177, 3622, 3886},
		},
		[DDR_933] = {
			.read_coef = {49, 87, 194, 348, 629,
	667, 719, 891, 930, 1065, 1146},
			.write_coef = {109, 205, 495, 812, 1579,
	1965, 2499, 3056, 3565, 4085, 4396},
		},
		[DDR_1066] = {
			.read_coef = {38, 78, 184, 319, 512,
	693, 816, 860, 910, 1037, 1127},
			.write_coef = {99, 204, 477, 909, 1644,
	2092, 2729, 3238, 3839, 4375, 4752},
		},
		[DDR_1600] = {
			.read_coef = {9, 22, 57, 101, 196,
	273, 373, 459, 550, 642, 695},
			.write_coef = {106, 188, 450, 883, 1745,
	2564, 3407, 4237, 5188, 6052, 6556},
		},
		[DDR_2133] = {
			.read_coef = {140, 168, 198, 239, 330,
	417, 514, 581, 680, 799, 865},
			.write_coef = {205, 329, 581, 991, 1837,
	2589, 3435, 4250, 5089, 5981, 6475},
		},
		[DDR_2750] = {
			.read_coef = {1510, 2000, 2022, 2057, 2116,
	2183, 2241, 2294, 2345, 2585, 2703},
			.write_coef = {1628, 2108, 2386, 2735, 3528,
	4170, 4969, 5436, 6150, 6778, 7087},
		},
	},
	.coef_idle = {76, 76, 82, 81, 88, 747, 763, 164},
	.coef_srst = {0, 8, 3, 6, 7, 7, 9, 0},
	.coef_ssr = { 8, 8, 3, 2, 10, 6, 10, 0},
	.volt = {600, 600, 600, 600, 600, 600, 600, 500},
	},
	[APHY_VM] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {20, 46, 104, 199, 359,
	457, 592, 650, 800, 919, 963},
			.write_coef = {16, 40, 96, 154, 312,
	348, 449, 522, 606, 696, 730},
		},
		[DDR_600] = {
			.read_coef = {40, 68, 150, 265, 506,
	680, 751, 983, 1134, 1284, 1380},
			.write_coef = {33, 65, 142, 251, 443,
	570, 735, 839, 959, 1086, 1167},
		},
		[DDR_800] = {
			.read_coef = {38, 67, 181, 323, 596,
	797, 938, 1151, 1361, 1517, 1619},
			.write_coef = {36, 72, 196, 347, 610,
	765, 905, 1046, 1237, 1378, 1471},
		},
		[DDR_933] = {
			.read_coef = {50, 81, 186, 333, 610,
	897, 968, 1267, 1472, 1655, 1761},
			.write_coef = {49, 80, 183, 294, 601,
	787, 972, 1173, 1383, 1555, 1654},
		},
		[DDR_1066] = {
			.read_coef = {34, 75, 181, 357, 709,
	931, 1108, 1367, 1639, 1879, 2012},
			.write_coef = {31, 77, 205, 327, 664,
	917, 1117, 1302, 1546, 1772, 1898},
		},
		[DDR_1600] = {
			.read_coef = {70, 133, 311, 524, 1060,
	1336, 1714, 1994, 2276, 2591, 2741},
			.write_coef = {68, 134, 259, 501, 991,
	1282, 1596, 1868, 2178, 2479, 2622},
		},
		[DDR_2133] = {
			.read_coef = {354, 620, 772, 979, 1396,
	1813, 2234, 2660, 3044, 3473, 3691},
			.write_coef = {476, 626, 754, 957, 1371,
	1752, 2154, 2513, 2937, 3350, 3561},
		},
		[DDR_2750] = {
			.read_coef = {466, 804, 1538, 2190, 3073,
	3524, 4157, 4535, 5047, 5474, 5730},
			.write_coef = {365, 747, 1736, 2063, 2818,
	3249, 3609, 4017, 4437, 4812, 5037},
		},
	},
	.coef_idle = {613, 629, 784, 712, 841, 860, 1212, 1334},
	.coef_srst = {70, 67, 70, 73, 76, 70, 77, 83},
	.coef_ssr = {24, 24, 24, 24, 23, 23, 24, 24},
	.volt = {750, 750, 750, 750, 750, 750, 750, 750},
	},
	[APHY_VIO12] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {64, 86, 175, 322, 586,
	827, 1078, 1446, 1446, 1446, 1446},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_600] = {
			.read_coef = {28, 62, 167, 293, 580,
	1045, 1086, 1209, 1626, 2043, 2461},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_800] = {
			.read_coef = {31, 85, 166, 357, 684,
	950, 1294, 1421, 1590, 1758, 1927},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_933] = {
			.read_coef = {28, 71, 167, 368, 743,
	903, 1157, 1523, 1758, 1993, 2228},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_1066] = {
			.read_coef = {39, 65, 189, 383, 564,
	961, 1330, 1495, 1741, 1988, 2234},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_1600] = {
			.read_coef = {45, 88, 210, 389, 777,
	900, 1170, 1497, 1672, 1847, 2022},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_2133] = {
			.read_coef = {71, 126, 304, 590, 1179,
	1567, 1717, 1741, 2126, 2510, 2895},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_2750] = {
			.read_coef = {92, 189, 472, 926, 1838,
	2189, 2539, 2743, 2753, 2763, 2773},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
	},
	.coef_idle = {120, 155, 127, 156, 171, 134, 162, 195},
	.coef_srst = {123, 135, 122, 131, 143, 115, 143, 200},
	.coef_ssr = { 0, 0, 0, 0, 0, 0, 0, 0},
	.volt = {1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
	},
};

static struct dram_pwr_data dram_def_pwr_conf[] = {
	[DRAM_VDD1] = {
	.idd_conf = {
#if DRAM_LP5
		[0] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 3770,	/* uA */
			.i_dd2p = 660,
			.i_dd2n = 129,
			.i_dd4w = 3370,
			.i_dd4r = 3070,
			.i_dd5 = 820,
			.i_dd6 = 316,
		},
		[1] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 4160,	/* uA */
			.i_dd2p = 700,
			.i_dd2n = 100,
			.i_dd4w = 4850,
			.i_dd4r = 4430,
			.i_dd5 = 800,
			.i_dd6 = 300,
		},
		[2] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 3970,	/* uA */
			.i_dd2p = 610,
			.i_dd2n = 190,
			.i_dd4w = 10270,
			.i_dd4r = 8920,
			.i_dd5 = 850,
			.i_dd6 = 275,
		},
#else
		[0] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 2940,	/* uA */
			.i_dd2p = 330,
			.i_dd2n = 0,
			.i_dd4w = 3140,
			.i_dd4r = 3710,
			.i_dd5 = 348,
			.i_dd6 = 275,
		},
		[1] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 2940,	/* uA */
			.i_dd2p = 330,
			.i_dd2n = 0,
			.i_dd4w = 3140,
			.i_dd4r = 3710,
			.i_dd5 = 348,
			.i_dd6 = 275,
		},
		[2] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 2940,	/* uA */
			.i_dd2p = 330,
			.i_dd2n = 0,
			.i_dd4w = 3140,
			.i_dd4r = 3710,
			.i_dd5 = 348,
			.i_dd6 = 275,
		},
#endif
	},
	},
	[DRAM_VDD2H] = {
	.idd_conf = {
		[0] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 36100,	/* uA */
			.i_dd2p = 1390,
			.i_dd2n = 11870,
			.i_dd4w = 171000,
			.i_dd4r = 183400,
			.i_dd5 = 17670,
			.i_dd6 = 625,
		},
		[1] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 36100,	/* uA */
			.i_dd2p = 1390,
			.i_dd2n = 11870,
			.i_dd4w = 171000,
			.i_dd4r = 183400,
			.i_dd5 = 17670,
			.i_dd6 = 625,
		},
		[2] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 36100,	/* uA */
			.i_dd2p = 1390,
			.i_dd2n = 11870,
			.i_dd4w = 171000,
			.i_dd4r = 183400,
			.i_dd5 = 17670,
			.i_dd6 = 625,
		},
	},
	},
	[DRAM_VDD2L] = {
#if DRAM_LP5
	.idd_conf = {
		[0] = {
			.volt = 900,	/* mV */
			.i_dd0 = 970,	/* uA */
			.i_dd2p = 20,
			.i_dd2n = 190,
			.i_dd4w = 29320,
			.i_dd4r = 31100,
			.i_dd5 = 250,
			.i_dd6 = 0,
		},
		[1] = {
			.volt = 900,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 0,
			.i_dd4r = 0,
			.i_dd5 = 0,
			.i_dd6 = 0,
		},
		[2] = {
			.volt = 900,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 0,
			.i_dd4r = 0,
			.i_dd5 = 0,
			.i_dd6 = 0,
		},
	},
#else
	.idd_conf = {
		[0] = {
			.volt = 0,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 0,
			.i_dd4r = 0,
			.i_dd5 = 0,
			.i_dd6 = 0,
		},
		[1] = {
			.volt = 0,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 0,
			.i_dd4r = 0,
			.i_dd5 = 0,
			.i_dd6 = 0,
		},
		[2] = {
			.volt = 0,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 0,
			.i_dd4r = 0,
			.i_dd5 = 0,
			.i_dd6 = 0,
		},
	},
#endif
	},
	[DRAM_VDDQ] = {
	.idd_conf = {
#if DRAM_LP5
		[0] = {
			.volt = 300,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 110,
			.i_dd4r = 6120,
			.i_dd5 = 50,
			.i_dd6 = 75,
		},
		[1] = {
			.volt = 300,	/* mV */
			.i_dd0 = 40,	/* uA */
			.i_dd2p = 40,
			.i_dd2n = 0,
			.i_dd4w = 2,
			.i_dd4r = 11200,
			.i_dd5 = 0,
			.i_dd6 = 75,
		},
		[2] = {
			.volt = 500,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 80,
			.i_dd2n = 0,
			.i_dd4w = 100,
			.i_dd4r = 57400,
			.i_dd5 = 20,
			.i_dd6 = 75,
		},
#else
		[0] = {
			.volt = 600,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 160,
			.i_dd4r = 76840,
			.i_dd5 = 20,
			.i_dd6 = 75,
		},
		[1] = {
			.volt = 600,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 160,
			.i_dd4r = 76840,
			.i_dd5 = 20,
			.i_dd6 = 75,
		},
		[2] = {
			.volt = 600,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 0,
			.i_dd2n = 0,
			.i_dd4w = 160,
			.i_dd4r = 76840,
			.i_dd5 = 20,
			.i_dd6 = 75,
		},
#endif
	},
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
static unsigned int swpm_get_avg_power(enum power_rail type)
{
	unsigned int *ptr;
	unsigned int cnt, idx, sum = 0, pwr = 0;

	if (type >= NR_POWER_RAIL) {
		pr_notice("Invalid SWPM type = %d\n", type);
		return 0;
	}

	/* TODO: avg_window data validation should be check in procfs */
#if SWPM_DEPRECATED
	/* window should be 1 to MAX_RECORD_CNT */
	avg_window = MAX(avg_window, 1);
	avg_window = MIN(avg_window, MAX_RECORD_CNT);
#endif

	/* get ptr of the target meter record */
	ptr = &swpm_info_ref->pwr[type][0];

	/* calculate avg */
	for (idx = swpm_info_ref->cur_idx, cnt = 0; cnt < avg_window; cnt++) {
		sum += ptr[idx];

		if (!idx)
			idx = MAX_RECORD_CNT - 1;
		else
			idx--;
	}

	pwr = sum / avg_window;

#if SWPM_TEST
	pr_notice("avg pwr of meter %d = %d uA\n", type, pwr);
#endif

	return pwr;
}

static void swpm_send_enable_ipi(unsigned int type, unsigned int enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && \
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_SWPM_ENABLE;
	qos_d.u.swpm_enable.type = type;
	qos_d.u.swpm_enable.enable = enable;
	qos_ipi_to_sspm_scmi_command(qos_d.cmd, type, enable, 0,
				     QOS_IPI_SCMI_SET);
#endif
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static void swpm_send_init_ipi(unsigned int addr, unsigned int size,
			      unsigned int ch_num)
{
#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;
	unsigned int offset;

	qos_d.cmd = QOS_IPI_SWPM_INIT;
	qos_d.u.swpm_init.dram_addr = addr;
	qos_d.u.swpm_init.dram_size = size;
	qos_d.u.swpm_init.dram_ch_num = ch_num;
	qos_ipi_to_sspm_scmi_command(qos_d.cmd, addr, size, ch_num,
					      QOS_IPI_SCMI_SET);
	offset = qos_ipi_to_sspm_scmi_command(qos_d.cmd, 0, 0, 0,
					      QOS_IPI_SCMI_GET);

	if (offset == -1) {
		pr_notice("qos ipi not ready init fail\n");
		goto error;
	} else if (offset == 0) {
		pr_notice("swpm share sram init fail\n");
		goto error;
	} else {
		pr_notice("swpm init offset = 0x%x\n", offset);
	}

	/* get wrapped sram address */
	wrap_d = (struct share_wrap *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!wrap_d) {
		pr_notice("swpm share sram offset fail\n");
		goto error;
	}

	/* get sram power index and control address from wrap data */
	share_idx_ref = (struct share_index *)
		sspm_sbuf_get(wrap_d->share_index_addr);
	share_idx_ctrl = (struct share_ctrl *)
		sspm_sbuf_get(wrap_d->share_ctrl_addr);

	/* init unsigned int ptr for share index iterator */
	idx_ref_uint_ptr = (unsigned int *)share_idx_ref;
	/* init output size to LTR except window_cnt with decrease 1 */
	idx_output_size =
		(sizeof(struct share_index)/sizeof(unsigned int)) - 1;

#if SWPM_TEST
	pr_notice("share_index size check = %zu bytes\n",
		 sizeof(struct share_index));
	pr_notice("wrap_d = 0x%p, wrap index addr = 0x%p, wrap ctrl addr = 0x%p\n",
		 wrap_d, wrap_d->share_index_addr, wrap_d->share_ctrl_addr);
	pr_notice("share_idx_ref = 0x%p, share_idx_ctrl = 0x%p\n",
		 share_idx_ref, share_idx_ctrl);
#endif

	swpm_init_state = 1;

	if (swpm_init_state)
		return;

error:
#endif
	swpm_init_state = 0;
	share_idx_ref = NULL;
	share_idx_ctrl = NULL;
	idx_ref_uint_ptr = NULL;
	idx_output_size = 0;
}

static inline void swpm_pass_to_sspm(void)
{
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), 2);
}

static void swpm_update_temp(void)
{
	unsigned int i;
	int tz_temp = 0, tz_core = 0;
	struct thermal_zone_device *tz;

	if (cpu_ptr) {
		for (i = 0; i < NR_CPU_CORE; i++) {
#if IS_ENABLED(CONFIG_MTK_THERMAL)
			tz_temp = get_cpu_temp(i);
			tz_temp = (tz_temp > 0) ? tz_temp : 0;
#endif
			cpu_ptr->cpu_temp[i] = (unsigned int)tz_temp;
		}
	}

	if (core_ptr) {
		for (i = 0; i < NR_CORE_TS; i++) {
			tz = thermal_zone_get_zone_by_name(core_ts_name[i]);
			if (!IS_ERR(tz)) {
				thermal_zone_get_temp(tz, &tz_temp);
				tz_core += tz_temp;
			}
		}
		tz_core = (tz_core > 0) ? tz_core / NR_CORE_TS : 0;
		core_ptr->thermal = (unsigned int)tz_core;
	}
}

static void swpm_idx_snap(void)
{
	unsigned long flags;

	if (share_idx_ref) {
		/* directly copy due to 8 bytes alignment problem */
		spin_lock_irqsave(&swpm_snap_spinlock, flags);
		mem_idx_snap.read_bw[0] = share_idx_ref->mem_idx.read_bw[0];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		spin_unlock_irqrestore(&swpm_snap_spinlock, flags);
	}
}

static void swpm_init_retry(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (!swpm_init_state)
		swpm_pass_to_sspm();
#endif
}

static char pwr_buf[POWER_CHAR_SIZE] = { 0 };
static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };
static void swpm_log_loop(struct timer_list *t)
{
	char *ptr = pwr_buf;
	char *idx_ptr = idx_buf;
	int i;
#ifdef LOG_LOOP_TIME_PROFILE
	ktime_t t1, t2;
	unsigned long long diff, diff2;

	t1 = ktime_get();
#endif

	/* initialization retry */
	if (swpm_init_retry_work_queue && !swpm_init_state)
		queue_work(swpm_init_retry_work_queue, &swpm_init_retry_work);

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			ptr += snprintf(ptr, 256, "%s/",
					swpm_power_rail[i].name);
		}
	}
	ptr--;
	ptr += sprintf(ptr, " = ");

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			swpm_power_rail[i].avg_power =
				swpm_get_avg_power((enum power_rail)i);
			ptr += snprintf(ptr, 256, "%d/",
					swpm_power_rail[i].avg_power);
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
	trace_swpm_power(pwr_buf);

#ifdef LOG_LOOP_TIME_PROFILE
	t2 = ktime_get();
#endif

	swpm_update_temp();

#ifdef LOG_LOOP_TIME_PROFILE
	diff = ktime_to_us(ktime_sub(t2, t1));
	diff2 = ktime_to_us(ktime_sub(ktime_get(), t2));
	pr_notice("exe time = %llu/%lluus\n", diff, diff2);
#endif

	mod_timer(t, jiffies + msecs_to_jiffies(swpm_log_interval_ms));
}

/* critical section function */
static void swpm_timer_init(void)
{
	swpm_lock(&swpm_mutex);

	swpm_timer.function = swpm_log_loop;
	timer_setup(&swpm_timer, swpm_log_loop, TIMER_DEFERRABLE);
#if SWPM_TEST
	/* Timer triggered by enable command */
	mod_timer(&swpm_timer, jiffies + msecs_to_jiffies(swpm_log_interval_ms));
#endif

	swpm_unlock(&swpm_mutex);
}

#endif /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) : 684 */

static void swpm_core_static_data_init(void)
{
	unsigned int static_p = 0, scaled_p;
	unsigned int i, j;

	if (!core_ptr)
		return;

#if IS_ENABLED(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE)
	/* TODO: default mA from get_devinfo */
	/* static_p = get_devinfo(CORE_STATIC_ID); */
#else
	static_p = 0;
#endif

	/* default CORE_STATIC mA */
	if (!static_p)
		static_p = CORE_STATIC_MA;

	/* recording default static data, and check replacement data */
	core_static = static_p;
	static_p = (!core_static_replaced) ?
		static_p : core_static_replaced;

	/* static power unit mA with voltage scaling */
	for (i = 0; i < NR_CORE_VOLT; i++) {
		scaled_p = static_p * core_volt_tbl[i] / V_OF_CORE_STATIC;
		for (j = 0; j < NR_CORE_STATIC_TYPE; j++) {
			/* calc static ratio and transfer unit to uA */
			core_ptr->core_static_pwr[i][j] =
				scaled_p * core_static_rt[j] / CORE_STATIC_RT_RES;
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

	pr_notice("aphy core_bw[%ld]/core[%ld]/core_volt[%ld]\n",
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
	memcpy(mem_ptr->dram_pwr_sample, dram_pwr_sample,
	       sizeof(dram_pwr_sample));
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


	pr_notice("ME disp_resolution=%d disp_fps=%d\n",
		me_ptr->disp_resolution, me_ptr->disp_fps);
}

static void swpm_init_pwr_data(void)
{
	int ret;
	phys_addr_t *ptr = NULL;

	ret = swpm_mem_addr_request(CPU_SWPM_TYPE, &ptr);
	if (!ret)
		cpu_ptr = (struct cpu_swpm_rec_data *)ptr;

	ret = swpm_mem_addr_request(CORE_SWPM_TYPE, &ptr);
	if (!ret)
		core_ptr = (struct core_swpm_rec_data *)ptr;

	ret = swpm_mem_addr_request(MEM_SWPM_TYPE, &ptr);
	if (!ret)
		mem_ptr = (struct mem_swpm_rec_data *)ptr;

	ret = swpm_mem_addr_request(ME_SWPM_TYPE, &ptr);
	if (!ret)
		me_ptr = (struct me_swpm_rec_data *)ptr;

	/* TBD static data flow */
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
			pr_notice("swpm_mem_tbl[%d] = 0x%x\n", i, ptr);

			/* data access test */
			switch (i) {
			case ISP_POWER_METER:
				isp_ptr = (struct isp_swpm_rec_data *)ptr;
				isp_ptr->isp_data[2] = 0x123;
				pr_notice("isp_data[0] = 0x%x\n",
					  isp_ptr->isp_data[0]);
				pr_notice("isp_data[1] = 0x%x\n",
					  isp_ptr->isp_data[1]);
				pr_notice("isp_data[2] = 0x%x\n",
					  isp_ptr->isp_data[2]);
				break;
			}
		} else {
			pr_notice("swpm_mem_tbl[%d] = NULL\n", i);
		}
	}
}
#endif

/* init share mem reference table for subsys */
static inline void swpm_subsys_data_ref_init(void)
{
	swpm_lock(&swpm_mutex);

	mem_ref_tbl[CPU_POWER_METER].valid = true;
	mem_ref_tbl[CPU_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->cpu_reserved;
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

#if SWPM_DEPRECATED
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
		pr_notice("disable swpm for data change, need to restart manually\n");
		swpm_set_enable(ALL_METER_TYPE, 0);
	}

	if (sscanf(buf, "%d %d %d", &type, &idd_idx, &val) == 3) {
		if (type < (NR_DRAM_PWR_TYPE) &&
		    idd_idx < (sizeof(struct dram_pwr_conf) /
			       sizeof(unsigned int)))
			*(&mem_ptr->dram_conf[type].i_dd0 + idd_idx) = val;
	} else {
		pr_notice("echo <type> <idx> <val> > /proc/swpm/idd_tbl\n");
	}

end:
	return count;
}

static int core_static_replace_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "default: %d, replaced %d (valid:0~999)\n",
		   core_static,
		   core_static_replaced);
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
		core_static_replaced = (val < 1000) ? val : core_static_replaced;

		/* reset core static power data */
		swpm_core_static_data_init();
	} else
		pr_notice("echo <val> > /proc/swpm/core_static_replace\n");

	return count;
}
#endif

static void swpm_cmd_dispatcher(unsigned int type,
				unsigned int val)
{
	switch (type) {
	case SET_PMU:
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

void swpm_set_update_cnt(unsigned int type, unsigned int cnt)
{
	if (!swpm_init_state
	    || (type != ALL_METER_TYPE && type >= NR_POWER_METER))
		return;

	swpm_lock(&swpm_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	{
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_SET_UPDATE_CNT;
		qos_d.u.swpm_set_update_cnt.type = type;
		qos_d.u.swpm_set_update_cnt.cnt = cnt;
		qos_ipi_to_sspm_scmi_command(qos_d.cmd, type, cnt, 0,
					     QOS_IPI_SCMI_SET);
	}
#endif
	swpm_unlock(&swpm_mutex);
}

int swpm_v6855_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	swpm_get_rec_addr(&rec_phys_addr,
			  &rec_virt_addr,
			  &rec_size);
	/* CONFIG_PHYS_ADDR_T_64BIT */
	swpm_info_ref = (struct swpm_rec_data *)rec_virt_addr;
#if SWPM_TEST
	pr_notice("rec_virt_addr = 0x%llx, swpm_info_ref = 0x%llx\n",
		  rec_virt_addr, swpm_info_ref);
#endif
#endif

	if (!swpm_info_ref) {
		pr_notice("get sspm dram addr failed\n");
		ret = -1;
		goto end;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	ret |= swpm_reserve_mem_init(&rec_virt_addr, &rec_size);
#endif

	swpm_subsys_data_ref_init();

	ret |= swpm_interface_manager_init(mem_ref_tbl, NR_SWPM_TYPE);

	swpm_init_pwr_data();

	swpm_core_ops_register(&plat_ops);

	if (!swpm_init_retry_work_queue) {
		swpm_init_retry_work_queue =
			create_workqueue("swpm_init_retry");
		if (!swpm_init_retry_work_queue)
			pr_debug("swpm_init_retry workqueue create failed\n");
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (!swpm_init_state)
		swpm_pass_to_sspm();
#endif

#if SWPM_TEST
	swpm_interface_unit_test();

	/* enable all pwr meter and set swpm timer to start */
	swpm_set_enable(ALL_METER_TYPE, EN_POWER_METER_ONLY);
#endif

	/* Only setup timer function */
	swpm_timer_init();
end:
	return ret;
}

void swpm_v6855_exit(void)
{
	swpm_lock(&swpm_mutex);

	del_timer_sync(&swpm_timer);
	swpm_set_enable(ALL_METER_TYPE, 0);

	swpm_unlock(&swpm_mutex);
}
