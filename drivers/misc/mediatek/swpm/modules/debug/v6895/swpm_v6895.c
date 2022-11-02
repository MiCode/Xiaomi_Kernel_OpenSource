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
#include <swpm_tracker_trace.h>
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power); */
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power_idx); */

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6895.h>
#include <swpm_v6895_ext.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
/* index snapshot */
DEFINE_SPINLOCK(swpm_snap_spinlock);
struct mem_swpm_index mem_idx_snap;
struct power_rail_data swpm_power_rail[NR_POWER_RAIL] = {
	[VPROC3] = {0, "VPROC3"},
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
#define CORE_STATIC_MA (96) /* 95.93 */
#define CORE_STATIC_ID (7)
#define CORE_STATIC_RT_RES (100)
#define V_OF_CORE_STATIC (750)
static char core_ts_name[NR_CORE_TS][MAX_POWER_NAME_LENGTH] = {
	"dram_ch2", "dram_ch0", "soc_top", "dram_ch3", "dram_ch1", "soc_mm"
};
static unsigned int core_static;
static unsigned int core_static_replaced;

static unsigned short core_static_rt[NR_CORE_STATIC_TYPE] = {
	528, 13725, 22558, 16756, 8714, 12925, 4233, 8191, 12371
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	575, 600, 650, 725, 750,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 933, 1066, 1333, 1600, 2133, 2750, 3200,
};
static unsigned short dram_pwr_sample[NR_DRAM_PWR_SAMPLE] = {
	800, 1333, 2750,
};
static struct aphy_core_bw_data aphy_ref_core_bw_tbl[] = {
	[DDR_400] = {
	.bw = {63, 142, 300, 566, 1300, 1929,
	2566, 3176, 3847, 4471, 5296, 5769},
	},
	[DDR_933] = {
	.bw = {141, 345, 1253, 3150, 6005, 6995,
	7859, 9607, 10375, 11222, 12230, 13759},
	},
	[DDR_1066] = {
	.bw = {175, 427, 1621, 4039, 7716, 8999,
	10061, 12290, 13249, 14410, 15652, 17662},
	},
	[DDR_1333] = {
	.bw = {175, 427, 1621, 4039, 7716, 8999,
	10061, 12290, 13249, 14410, 15652, 17662},
	},
	[DDR_1600] = {
	.bw = {273, 670, 2535, 6338, 12021, 13892,
	15507, 18910, 20276, 22132, 23896, 27176},
	},
	[DDR_2133] = {
	.bw = {321, 786, 2957, 7416, 13995, 16105,
	17957, 21886, 23383, 25861, 27678, 31358},
	},
	[DDR_2750] = {
	.bw = {413, 1013, 3813, 9561, 18043, 20764,
	23151, 28216, 30147, 33342, 35685, 40429},
	},
	[DDR_3200] = {
	.bw = {481, 1179, 4436, 11125, 20995, 24161,
	26939, 32833, 35080, 38798, 41524, 47044},
	},
};
static struct aphy_core_pwr_data aphy_def_core_pwr_tbl[] = {
	[APHY_VCORE] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {9, 21, 43, 81, 171, 249,
			324, 402, 450, 486, 486, 496},
			.write_coef = {16, 37, 77, 145, 322, 469,
			612, 770, 912, 972, 972, 972},
		},
		[DDR_933] = {
			.read_coef = {121, 306, 859, 1387, 1932, 2098,
			2206, 2214, 2214, 2214, 2214, 2214},
			.write_coef = {127, 324, 998, 1754, 2766, 3018,
			3231, 3235, 3241, 3241, 3241, 3246},
		},
		[DDR_1066] = {
			.read_coef = {149, 352, 976, 1588, 2154, 2316,
			2430, 2458, 2458, 2458, 2458, 2458},
			.write_coef = {168, 360, 1155, 2029, 3127, 3430,
			3536, 3536, 3536, 3536, 3545, 3545},
		},
		[DDR_1333] = {
			.read_coef = {186, 441, 1221, 1985, 2694, 2896,
			3039, 3074, 3074, 3074, 3074, 3074},
			.write_coef = {210, 450, 1444, 2537, 3910, 4289,
			4422, 4422, 4422, 4422, 4433, 4433},
		},
		[DDR_1600] = {
			.read_coef = {445, 1034, 2685, 4125, 5352, 5753,
			5932, 5980, 5982, 6006, 6006, 6006},
			.write_coef = {455, 1016, 2958, 5112, 7376, 8026,
			8583, 8614, 8614, 8622, 8652, 8682},
		},
		[DDR_2133] = {
			.read_coef = {686, 1493, 3862, 5960, 7489, 7989,
			8228, 8378, 8401, 8422, 8422, 8422},
			.write_coef = {709, 1469, 4225, 7417, 10319, 11197,
			11992, 12059, 12059, 12121, 12121, 12121},
		},
		[DDR_2750] = {
			.read_coef = {798, 1634, 4748, 7503, 9595, 10296,
			10607, 10799, 10829, 10859, 10859, 10859},
			.write_coef = {914, 1894, 5448, 9562, 13304, 14436,
			15460, 15547, 15547, 15627, 15627, 15627},
		},
		[DDR_3200] = {
			.read_coef = {994, 2035, 5912, 9343, 11948, 12821,
			13209, 13447, 13485, 13522, 13522, 13522},
			.write_coef = {1138, 2358, 6784, 11907, 16567, 17977,
			19252, 19360, 19360, 19460, 19460, 19460},
		},
	},
	.coef_idle = {1191, 1778, 1587, 1984, 3129, 4460, 5750, 7161},
	.coef_srst = {168, 168, 150, 187, 249, 326, 346, 369},
	.coef_ssr = {35, 35, 29, 37, 45, 53, 53, 57},
	.volt = {550, 550, 600, 600, 650, 725, 750, 750},
	},
};

static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
	.bw = {64, 128, 192, 256,
	320, 640, 960, 1280, 1600, 1920,
	2547, 3187, 3814, 4480, 5069, 5722},
	},
	[DDR_933] = {
	.bw = {149, 299, 448, 597,
	746, 1493, 2239, 2986, 3702, 4434,
	5971, 7315, 8867, 10450, 11868, 13241},
	},
	[DDR_1066] = {
	.bw = {192, 384, 576, 768,
	960, 1920, 2861, 3840, 4762, 5683,
	7584, 9542, 11386, 13440, 14976, 17280},
	},
	[DDR_1333] = {
	.bw = {192, 384, 576, 768,
	960, 1920, 2861, 3840, 4762, 5683,
	7584, 9542, 11386, 13440, 14976, 17280},
	},
	[DDR_1600] = {
	.bw = {299, 597, 896, 1194,
	1493, 2986, 4478, 5971, 7315, 8867,
	11704, 14629, 17227, 20899, 23736, 26363},
	},
	[DDR_2133] = {
	.bw = {341, 683, 1024, 1365,
	1706, 3413, 5119, 6826, 8361, 10136,
	13378, 16723, 19692, 23890, 27132, 30135},
	},
	[DDR_2750] = {
	.bw = {440, 880, 1320, 1760,
	2200, 4400, 6600, 8800, 10780, 13068,
	17248, 21560, 25388, 30800, 34980, 38852},
	},
	[DDR_3200] = {
	.bw = {512, 1024, 1536, 2048,
	2560, 5120, 7680, 10240, 12544, 15206,
	20070, 25088, 29542, 35840, 40704, 45210},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {

	[APHY_VDDQ] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {2, 2, 2, 2,
			2, 2, 2, 2, 2, 2,
			2, 2, 2, 2, 2, 2},
			.write_coef = {1, 1, 5, 8,
			12, 30, 47, 62, 74, 87,
			114, 138, 162, 192, 213, 233},
		},
		[DDR_933] = {
			.read_coef = {13, 20, 27, 31,
			36, 49, 55, 57, 60, 60,
			60, 60, 60, 60, 60, 60},
			.write_coef = {23, 44, 65, 83,
			98, 170, 232, 302, 361, 406,
			503, 587, 674, 754, 839, 895},
		},
		[DDR_1066] = {
			.read_coef = {13, 20, 27, 32,
			37, 50, 57, 58, 61, 61,
			61, 62, 61, 61, 61, 61},
			.write_coef = {24, 46, 67, 86,
			101, 175, 237, 311, 371, 416,
			511, 613, 692, 775, 847, 934},
		},
		[DDR_1333] = {
			.read_coef = {16, 25, 34, 40,
			46, 63, 71, 73, 77, 77,
			76, 78, 77, 77, 77, 77},
			.write_coef = {30, 57, 84, 107,
			127, 219, 296, 389, 464, 520,
			639, 766, 866, 970, 1059, 1168},
		},
		[DDR_1600] = {
			.read_coef = {77, 80, 87, 94,
			100, 106, 121, 127, 149, 268,
			660, 1172, 1619, 2260, 2779, 3241},
			.write_coef = {156, 282, 378, 530,
			613, 969, 1278, 1573, 1847, 2125,
			2634, 3120, 3546, 4111, 4494, 4720},
		},
		[DDR_2133] = {
			.read_coef = {115, 116, 121, 130,
			138, 145, 154, 164, 186, 305,
			666, 1148, 1530, 2234, 2726, 3168},
			.write_coef = {213, 375, 479, 692,
			783, 1121, 1425, 1700, 1942, 2225,
			2667, 3125, 3475, 4117, 4542, 4558},
		},
		[DDR_2750] = {
			.read_coef = {149, 149, 156, 167,
			177, 186, 199, 211, 240, 393,
			858, 1480, 1972, 2881, 3515, 4084},
			.write_coef = {274, 483, 618, 892,
			1010, 1445, 1837, 2192, 2503, 2869,
			3438, 4029, 4480, 5307, 5855, 5877},
		},
		[DDR_3200] = {
			.read_coef = {173, 174, 182, 195,
			206, 217, 231, 246, 280, 458,
			999, 1722, 2295, 3352, 4090, 4752},
			.write_coef = {319, 563, 719, 1038,
			1175, 1682, 2138, 2550, 2913, 3338,
			4001, 4688, 5213, 6176, 6814, 6839},
		},
	},
	.coef_idle = {26, 26, 21, 26, 1043, 1000, 1000, 1000},
	.coef_srst = {5, 5, 4, 5, 14, 13, 13, 13},
	.coef_ssr = {4, 4, 3, 4, 12, 12, 12, 12},
	.volt = {300, 300, 300, 300, 500, 500, 500, 500},
	},
	[APHY_VM] = {
	.pwr = {
		[DDR_400] = {
		.read_coef = {53, 105, 158, 211,
		263, 264, 452, 640, 687, 734,
		825, 833, 945, 1105, 1250, 1412},
		.write_coef = {49, 99, 144, 189,
		234, 191, 309, 428, 435, 442,
		445, 435, 475, 521, 554, 619},
		},
		[DDR_933] = {
		.read_coef = {131, 257, 373, 488,
		604, 663, 1056, 1459, 1587, 1706,
		1963, 2130, 2645, 3117, 3560, 3950},
		.write_coef = {125, 259, 376, 493,
		610, 649, 875, 1101, 1211, 1318,
		1503, 1670, 1959, 2257, 2309, 2462},
		},
		[DDR_1066] = {
		.read_coef = {211, 411, 597, 783,
		895, 1499, 2154, 2642, 3055, 3470,
		4117, 4897, 5842, 6897, 7857, 8867},
		.write_coef = {215, 407, 591, 775,
		883, 1391, 1842, 2224, 2545, 2810,
		3294, 4088, 4356, 4879, 5437, 6273},
		},
		[DDR_1333] = {
		.read_coef = {150, 322, 472, 622,
		772, 1203, 1744, 2303, 2711, 3118,
		3889, 4591, 5076, 5987, 6821, 7697},
		.write_coef = {155, 280, 417, 554,
		691, 1086, 1451, 1829, 2139, 2450,
		2996, 3690, 4358, 4524, 4714, 5439},
		},
		[DDR_1600] = {
		.read_coef = {224, 421, 620, 818,
		964, 1622, 2362, 2984, 3474, 4016,
		5009, 5790, 6701, 8130, 9284, 10255},
		.write_coef = {215, 395, 584, 774,
		906, 1432, 1911, 2370, 2770, 3149,
		4080, 4988, 5398, 5809, 6536, 7260},
		},
		[DDR_2133] = {
		.read_coef = {1808, 3402, 3868, 4335,
		4802, 5437, 6018, 6599, 7168, 7825,
		8832, 9515, 10062, 11722, 13386, 14786},
		.write_coef = {1776, 3391, 3823, 4255,
		4687, 5130, 5596, 6061, 6443, 6885,
		7657, 8156, 8541, 9110, 9596, 10658},
		},
		[DDR_2750] = {
		.read_coef = {2231, 4191, 4798, 5406,
		6013, 6731, 7474, 8217, 8881, 9641,
		10878, 11847, 12501, 14569, 16638, 18378},
		.write_coef = {2215, 4139, 4692, 5245,
		5797, 6357, 6919, 7479, 7925, 8435,
		9243, 10092, 10594, 11371, 12137, 13417},
		},
		[DDR_3200] = {
		.read_coef = {2611, 4790, 5489, 6188,
		6887, 7715, 8549, 9383, 10150, 11027,
		12403, 13413, 14131, 16949, 19355, 21380},
		.write_coef = {2548, 4784, 5419, 6054,
		6690, 7343, 7995, 8646, 9232, 9898,
		10866, 11563, 12166, 13113, 13739, 15260},
		},
	},
	.coef_idle = {1009, 2225, 2427, 2470, 2868, 5081, 6194, 7096},
	.coef_srst = {296, 336, 428, 428, 446, 657, 657, 657},
	.coef_ssr = {0, 0, 0, 0, 0, 0, 0, 0},
	.volt = {575, 600, 650, 650, 650, 750, 750, 750},
	},
	[APHY_VIO12] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {68, 120, 172, 221,
			264, 480, 720, 936, 1164, 1380,
			1716, 2004, 2496, 2844, 3156, 3300},
			.write_coef = {1, 5, 6, 9,
			11, 12, 13, 14, 16, 17,
			18, 19, 20, 22, 23, 23},
			},
		[DDR_933] = {
			.read_coef = {72, 144, 216, 288,
			348, 684, 998, 1236, 1584, 1716,
			2124, 2388, 2784, 3096, 3389, 3631},
			.write_coef = {12, 12, 12, 24,
			48, 48, 48, 48, 48, 48,
			48, 48, 48, 48, 48, 48},
		},
		[DDR_1066] = {
			.read_coef = {58, 115, 173, 230,
			278, 547, 797, 988, 1267, 1372,
			1699, 1910, 2226, 2476, 2705, 2947},
			.write_coef = {10, 10, 10, 19,
			38, 38, 38, 38, 38, 38,
			38, 38, 38, 38, 38, 38},
		},
		[DDR_1333] = {
			.read_coef = {72, 144, 216, 288,
			348, 684, 996, 1236, 1584, 1716,
			2124, 2388, 2784, 3096, 3383, 3685},
			.write_coef = {12, 12, 12, 24,
			48, 48, 48, 48, 48, 48,
			48, 48, 48, 48, 48, 48},
		},
		[DDR_1600] = {
			.read_coef = {113, 213, 300, 401,
			476, 889, 1289, 1690, 1865, 2115,
			2441, 2854, 3254, 3567, 3820, 3999},
			.write_coef = {13, 25, 38, 63,
			63, 63, 63, 63, 63, 63,
			63, 63, 63, 63, 63, 63},
		},
		[DDR_2133] = {
			.read_coef = {120, 216, 288, 408,
			468, 852, 1224, 1668, 1788, 2064,
			2352, 2712, 3096, 3408, 3617, 3760},
			.write_coef = {12, 24, 36, 60,
			60, 60, 60, 60, 60, 60,
			60, 60, 60, 60, 60, 60},
		},
		[DDR_2750] = {
			.read_coef = {120, 216, 288, 408,
			468, 852, 1224, 1668, 1788, 2064,
			2352, 2712, 3096, 3408, 3617, 3760},
			.write_coef = {12, 24, 36, 60,
			60, 60, 60, 60, 60, 60,
			60, 60, 60, 60, 60, 60},
		},
		[DDR_3200] = {
			.read_coef = {120, 216, 288, 408,
			468, 852, 1224, 1668, 1788, 2064,
			2352, 2712, 3096, 3408, 3617, 3760},
			.write_coef = {12, 24, 36, 60,
			60, 60, 60, 60, 60, 60,
			60, 60, 60, 60, 60, 60},
		},
	},
	.coef_idle = {444, 516, 523, 654, 1327, 1489, 1919, 2234},
	.coef_srst = {186, 323, 327, 409, 444, 490, 632, 736},
	.coef_ssr = {1, 1, 1, 1, 2, 1, 1, 1},
	.volt = {1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
	},
};

/* IDD table */
static struct dram_pwr_data dram_def_pwr_conf[] = {
	[DRAM_VDD1] = {
	.idd_conf = {
		/* 1600MHz */
		[0] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 6786,	/* uA */
			.i_dd2p = 1193,
			.i_dd2n = 216,
			.i_dd4w = 6048,
			.i_dd4r = 5580,
			.i_dd5 = 1476,
			.i_dd6 = 495,
		},
		/* 2667MHz */
		[1] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 7488,	/* uA */
			.i_dd2p = 1224,
			.i_dd2n = 216,
			.i_dd4w = 8712,
			.i_dd4r = 7974,
			.i_dd5 = 1440,
			.i_dd6 = 540,
		},
		/* 5500MHz */
		[2] = {
			.volt = 1800,	/* mV */
			.i_dd0 = 7146,	/* uA */
			.i_dd2p = 1098,
			.i_dd2n = 342,
			.i_dd4w = 18486,
			.i_dd4r = 16020,
			.i_dd5 = 1530,
			.i_dd6 = 495,
		},
	},
	},
	[DRAM_VDD2H] = {
	.idd_conf = {
		/* 1600MHz */
		[0] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 38640,	/* uA */
			.i_dd2p = 2415,
			.i_dd2n = 7402,
			.i_dd4w = 65520,
			.i_dd4r = 67410,
			.i_dd5 = 5922,
			.i_dd6 = 761,
		},
		/* 2667MHz */
		[1] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 35385,	/* uA */
			.i_dd2p = 2383,
			.i_dd2n = 7728,
			.i_dd4w = 155620,
			.i_dd4r = 165406,
			.i_dd5 = 6132,
			.i_dd6 = 761,
		},
		/* 5500MHz */
		[2] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 42714,	/* uA */
			.i_dd2p = 3234,
			.i_dd2n = 21745,
			.i_dd4w = 264285,
			.i_dd4r = 294808,
			.i_dd5 = 6426,
			.i_dd6 = 761,
		},
	},
	},
	[DRAM_VDD2L] = {
	.idd_conf = {
		/* 1600MHz */
		[0] = {
			.volt = 900,	/* mV */
			.i_dd0 = 873,	/* uA */
			.i_dd2p = 9,
			.i_dd2n = 180,
			.i_dd4w = 26370,
			.i_dd4r = 27900,
			.i_dd5 = 225,
			.i_dd6 = 0,
		},
		/* 2667MHz */
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
		/* 5500MHz */
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
	},
	[DRAM_VDDQ] = {
	.idd_conf = {
		[0] = {
			.volt = 300,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 30,
			.i_dd2n = 0,
			.i_dd4w = 33,
			.i_dd4r = 1836,
			.i_dd5 = 15,
			.i_dd6 = 22,
		},
		[1] = {
			.volt = 300,	/* mV */
			.i_dd0 = 12,	/* uA */
			.i_dd2p = 12,
			.i_dd2n = 0,
			.i_dd4w = 3,
			.i_dd4r = 3381,
			.i_dd5 = 0,
			.i_dd6 = 22,
		},
		[2] = {
			.volt = 500,	/* mV */
			.i_dd0 = 0,	/* uA */
			.i_dd2p = 40,
			.i_dd2n = 0,
			.i_dd4w = 45,
			.i_dd4r = 28690,
			.i_dd5 = 12,
			.i_dd6 = 37,
		},
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
		mem_idx_snap.read_bw[1] = share_idx_ref->mem_idx.read_bw[1];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		mem_idx_snap.write_bw[1] = share_idx_ref->mem_idx.write_bw[1];
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

unsigned int swpm_core_static_data_get(void)
{
	return core_static;
}

void swpm_core_static_replaced_data_set(unsigned int data)
{
	core_static_replaced = data;
}

void swpm_core_static_data_init(void)
{
	unsigned int static_p = 0, scaled_p;
	unsigned int i, j;

	if (!core_ptr)
		return;

	/* TODO: default mA from get_devinfo */
#if IS_ENABLED(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE)
	static_p = get_devinfo(CORE_STATIC_ID);
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

int swpm_v6895_init(void)
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

void swpm_v6895_exit(void)
{
	swpm_lock(&swpm_mutex);

	del_timer_sync(&swpm_timer);
	swpm_set_enable(ALL_METER_TYPE, 0);

	swpm_unlock(&swpm_mutex);
}
