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
#include <swpm_tracker_trace.h>
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power); */
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power_idx); */

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6879.h>
#include <swpm_v6879_ext.h>

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
#define CORE_STATIC_MA (34)
#define CORE_STATIC_ID (7)
#define CORE_STATIC_RT_RES (100)
#define V_OF_CORE_STATIC (750)
static char core_ts_name[NR_CORE_TS][MAX_POWER_NAME_LENGTH] = {
	"soc1", "soc2", "soc3", "soc4",
};
static unsigned int core_static;
static unsigned int core_static_replaced;
static unsigned short core_static_rt[NR_CORE_STATIC_TYPE] = {
	6893, 7823, 8187, 14263, 15893, 18254, 28687,
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	550, 600, 650, 750,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1066, 1600, 2133, 2750,
};
static unsigned short dram_pwr_sample[NR_DRAM_PWR_SAMPLE] = {
	800, 1333, 2750,
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
			.read_coef = {4, 10, 20, 38, 80, 117,
	152, 189, 212, 229, 229, 234},
			.write_coef = {8, 17, 36, 68, 152, 221,
	288, 363, 429, 458, 458, 458},
		},
		[DDR_600] = {
			.read_coef = {5, 14, 49, 117, 212, 253,
	267, 276, 278, 280, 281, 281},
			.write_coef = {13, 32, 119, 291, 560, 613,
	684, 684, 684, 684, 684, 686},
		},
		[DDR_800] = {
			.read_coef = {48, 120, 338, 545, 773, 838,
	886, 887, 888, 889, 889, 889},
			.write_coef = {55, 125, 406, 715, 1131, 1233,
	1277, 1277, 1277, 1277, 1279, 1281},
		},
		[DDR_933] = {
			.read_coef = {61, 158, 442, 714, 995, 1080,
	1135, 1139, 1139, 1139, 1139, 1139},
			.write_coef = {65, 167, 513, 903, 1424, 1553,
	1663, 1665, 1668, 1668, 1668, 1671},
		},
		[DDR_1066] = {
			.read_coef = {70, 178, 501, 808, 1126, 1223,
	1286, 1291, 1291, 1291, 1291, 1291},
			.write_coef = {74, 189, 582, 1023, 1613, 1759,
	1884, 1886, 1889, 1889, 1889, 1892},
		},
		[DDR_1600] = {
			.read_coef = {157, 395, 1024, 1565, 2098, 2272,
	2345, 2347, 2348, 2349, 2357, 2381},
			.write_coef = {160, 385, 1135, 1932, 2891, 3155,
	3369, 3374, 3378, 3378, 3378, 3426},
		},
		[DDR_2133] = {
			.read_coef = {309, 675, 1746, 2695, 3386, 3612,
	3720, 3788, 3798, 3808, 3808, 3808},
			.write_coef = {320, 664, 1910, 3353, 4666, 5063,
	5422, 5452, 5452, 5480, 5480, 5480},
		},
		[DDR_2750] = {
			.read_coef = {514, 1115, 2893, 4466, 5612, 5987,
	6166, 6277, 6295, 6311, 6311, 6311},
			.write_coef = {531, 1100, 3166, 5557, 7732, 8390,
	8985, 9036, 9036, 9082, 9082, 9082},
		},
	},
	.coef_idle = {590, 694, 799, 963, 1137, 1138, 2641, 3810},
	.coef_srst = {83, 83, 83, 92, 100, 141, 193, 259},
	.coef_ssr = {17, 17, 17, 18, 20, 25, 31, 36},
	.volt = {550, 550, 550, 550, 550, 600, 650, 750},
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
			.read_coef = {4, 7, 15, 24, 44,
	59, 68, 73, 91, 105, 112},
			.write_coef = {5, 13, 27, 52, 95,
	138, 182, 225, 270, 312, 333},
		},
		[DDR_600] = {
			.read_coef = {1, 5, 16, 33, 66,
	84, 110, 112, 155, 179, 192},
			.write_coef = {11, 17, 38, 77, 151,
	215, 286, 346, 421, 486, 520},
		},
		[DDR_800] = {
			.read_coef = {3, 9, 24, 44, 82,
	112, 147, 166, 185, 219, 235},
			.write_coef = {13, 22, 52, 108, 198,
	278, 364, 445, 527, 624, 670},
		},
		[DDR_933] = {
			.read_coef = {15, 23, 65, 112, 197,
	260, 341, 391, 407, 471, 504},
			.write_coef = {20, 44, 98, 172, 323,
	440, 563, 673, 797, 922, 987},
		},
		[DDR_1066] = {
			.read_coef = {17, 33, 74, 135, 234,
	285, 390, 425, 463, 530, 564},
			.write_coef = {25, 52, 103, 211, 364,
	509, 647, 763, 933, 1068, 1135},
		},
		[DDR_1600] = {
			.read_coef = {25, 45, 98, 173, 296,
	383, 469, 518, 585, 668, 722},
			.write_coef = {37, 72, 142, 278, 491,
	655, 843, 1018, 1203, 1373, 1484},
		},
		[DDR_2133] = {
			.read_coef = {1751, 1819, 1835, 1856, 1896,
	1951, 1995, 2040, 2088, 2282, 2398},
			.write_coef = {1835, 1972, 2206, 2572, 3291,
	4039, 4710, 5218, 5982, 6538, 6872},
		},
		[DDR_2750] = {
			.read_coef = {1510, 2000, 2022, 2057, 2116,
	2183, 2241, 2294, 2345, 2585, 2703},
			.write_coef = {1628, 2108, 2386, 2735, 3528,
	4170, 4969, 5436, 6150, 6778, 7087},
		},
	},
	.coef_idle = {12, 12, 12, 13, 13, 14, 114, 164},
	.coef_srst = {0, 0, 0, 0, 0, 0, 0, 0},
	.coef_ssr = {0, 0, 0, 0, 0, 0, 0, 0},
	.volt = {300, 300, 300, 300, 300, 300, 500, 500},
	},
	[APHY_VM] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {29, 52, 122, 212, 393,
	562, 656, 758, 901, 1035, 1079},
			.write_coef = {39, 52, 107, 168, 258,
	353, 442, 482, 578, 664, 693},
		},
		[DDR_600] = {
			.read_coef = {35, 62, 161, 270, 534,
	777, 987, 1014, 1266, 1401, 1508},
			.write_coef = {31, 52, 117, 197, 388,
	502, 653, 752, 907, 1003, 1080},
		},
		[DDR_800] = {
			.read_coef = {43, 91, 218, 362, 649,
	884, 1100, 1222, 1489, 1692, 1784},
			.write_coef = {28, 76, 178, 286, 510,
	651, 840, 992, 1163, 1321, 1393},
		},
		[DDR_933] = {
			.read_coef = {54, 96, 229, 399, 762,
	984, 1270, 1635, 1865, 2111, 2248},
			.write_coef = {43, 66, 202, 374, 617,
	796, 995, 1128, 1397, 1581, 1684},
		},
		[DDR_1066] = {
			.read_coef = {60, 100, 239, 457, 855,
	1068, 1328, 1731, 1948, 2245, 2384},
			.write_coef = {47, 86, 201, 366, 639,
	889, 1139, 1287, 1542, 1777, 1888},
		},
		[DDR_1600] = {
			.read_coef = {60, 122, 308, 568, 1066,
	1501, 1965, 2381, 2722, 2981, 3227},
			.write_coef = {48, 111, 259, 470, 841,
	1144, 1457, 1739, 2168, 2374, 2570},
		},
		[DDR_2133] = {
			.read_coef = {145, 309, 683, 1222, 2218,
	2481, 3300, 3646, 4011, 4360, 4542},
			.write_coef = {169, 297, 713, 1345, 1792,
	2485, 2836, 3107, 3447, 3747, 3903},
		},
		[DDR_2750] = {
			.read_coef = {466, 804, 1538, 2190, 3073,
	3524, 4157, 4535, 5047, 5474, 5730},
			.write_coef = {365, 747, 1736, 2063, 2818,
	3249, 3609, 4017, 4437, 4812, 5037},
		},
	},
	.coef_idle = {617, 637, 782, 782, 900, 779, 1040, 1334},
	.coef_srst = {70, 67, 70, 73, 76, 70, 77, 83},
	.coef_ssr = {24, 24, 24, 24, 23, 23, 24, 24},
	.volt = {750, 750, 750, 750, 750, 750, 750, 750},
	},
	[APHY_VIO12] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {61, 93, 219, 412, 835,
	1181, 1275, 1363, 1824, 2285, 2746},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_600] = {
			.read_coef = {39, 75, 201, 398, 809,
	1041, 1474, 1763, 2053, 2342, 2632},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_800] = {
			.read_coef = {40, 86, 192, 399, 756,
	1181, 1359, 1589, 1884, 2180, 2475},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_933] = {
			.read_coef = {60, 93, 212, 416, 729,
	1098, 1500, 1698, 1723, 1748, 1773},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_1066] = {
			.read_coef = {55, 96, 222, 425, 847,
	1120, 1589, 1626, 1726, 1827, 1927},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_1600] = {
			.read_coef = {70, 110, 258, 489, 897,
	1219, 1577, 1630, 1878, 2125, 2373},
			.write_coef = {0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0},
		},
		[DDR_2133] = {
			.read_coef = {102, 186, 451, 785, 1459,
	2251, 2617, 2690, 2733, 2776, 2820},
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
	.coef_idle = {120, 144, 124, 151, 160, 126, 156, 195},
	.coef_srst = {120, 144, 114, 145, 158, 129, 152, 200},
	.coef_ssr = {0, 0, 0, 0, 0, 0, 0, 0},
	.volt = {1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
	},
};

static struct dram_pwr_data dram_def_pwr_conf[] = {
	[DRAM_VDD1] = {
	.idd_conf = {
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
	},
	},
	[DRAM_VDD2H] = {
	.idd_conf = {
		[0] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 36800,	/* uA */
			.i_dd2p = 2300,
			.i_dd2n = 7050,
			.i_dd4w = 62400,
			.i_dd4r = 64300,
			.i_dd5 = 5640,
			.i_dd6 = 833,
		},
		[1] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 33700,	/* uA */
			.i_dd2p = 2270,
			.i_dd2n = 7360,
			.i_dd4w = 148200,
			.i_dd4r = 157500,
			.i_dd5 = 5840,
			.i_dd6 = 725,
		},
		[2] = {
			.volt = 1050,	/* mV */
			.i_dd0 = 40600,	/* uA */
			.i_dd2p = 3100,
			.i_dd2n = 20700,
			.i_dd4w = 251700,
			.i_dd4r = 280700,
			.i_dd5 = 6120,
			.i_dd6 = 725,
		},
	},
	},
	[DRAM_VDD2L] = {
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
	},
	[DRAM_VDDQ] = {
	.idd_conf = {
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

int swpm_v6879_init(void)
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

void swpm_v6879_exit(void)
{
	swpm_lock(&swpm_mutex);

	del_timer_sync(&swpm_timer);
	swpm_set_enable(ALL_METER_TYPE, 0);

	swpm_unlock(&swpm_mutex);
}
