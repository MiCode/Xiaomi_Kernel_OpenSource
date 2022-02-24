// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
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
#include <mach/mtk_cpufreq_api.h>
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
#include <mtk_swpm_interface.h>


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
static DEFINE_SPINLOCK(swpm_snap_spinlock);
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

__weak int mt_spower_get_leakage_uW(int dev, int voltage, int deg)
{
	return 0;
}
static struct core_swpm_rec_data *core_ptr;
static struct mem_swpm_rec_data *mem_ptr;
static struct me_swpm_rec_data *me_ptr;

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
#define CORE_DEFAULT_LKG (64)
#define CORE_LKG_RT_RES (100)
static unsigned int core_lkg;
static unsigned int core_lkg_replaced;
static unsigned short core_lkg_rt[NR_CORE_LKG_TYPE] = {
	6764, 562, 1629, 1213, 7155, 17155,
	11274, 3152, 4835, 3687, 2487, 13547,
};
static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	550, 600, 650, 725,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1200, 1600, 2133,
};
static struct aphy_core_bw_data aphy_ref_core_bw_tbl[] = {
	[DDR_400] = {
	.bw = {
		32, 71, 150, 283, 650, 964,
	1283, 1588, 1923, 2235, 2648, 2884},
	},
	[DDR_600] = {
	.bw = {
		46, 107, 398, 1013, 1929, 2246,
	2521, 3086, 3350, 3631, 3946, 4398},
	},
	[DDR_800] = {
	.bw = {
		60, 142, 538, 1349, 2572, 2995,
	3369, 4133, 4450, 4825, 5221, 5897},
	},
	[DDR_933] = {
	.bw = {
		70, 173, 626, 1575, 3002, 3497,
	3929, 4803, 5187, 5611, 6115, 6880},
	},
	[DDR_1200] = {
	.bw = {
		87, 213, 811, 2020, 3858, 4500,
	5030, 6145, 6624, 7205, 7826, 8831},
	},
	[DDR_1600] = {
	.bw = {
		118, 292, 1109, 2765, 5270, 6116,
	6835, 8339, 8973, 9667, 10530, 12019},
	},
	[DDR_2133] = {
	.bw = {
		160, 393, 1479, 3708, 6997, 8053,
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
			.read_coef = {108, 279, 779, 1258, 1753, 1904,
	2002, 2009, 2009, 2009, 2009, 2009},
			.write_coef = {115, 294, 905, 1592, 2510, 2738,
	2932, 2936, 2940, 2940, 2940, 2945},
		},
		[DDR_1200] = {
			.read_coef = {169, 406, 1058, 1685, 2255, 2416,
	2532, 2562, 2562, 2562, 2562, 2562},
			.write_coef = {175, 375, 1203, 2114, 3259, 3574,
	3685, 3685, 3685, 3685, 3694, 3694},
		},
		[DDR_1600] = {
			.read_coef = {252, 635, 1643, 2513, 3367, 3646,
	3764, 3766, 3768, 3770, 3783, 3821},
			.write_coef = {257, 618, 1822, 3101, 4640, 5063,
	5406, 5415, 5422, 5422, 5422, 5498},
		},
		[DDR_2133] = {
			.read_coef = {488, 1065, 2756, 4252, 5343, 5700,
	5870, 5977, 5993, 6009, 6009, 6009},
			.write_coef = {506, 1048, 3015, 5291, 7362, 7989,
	8555, 8603, 8603, 8648, 8648, 8648},
		},
	},
	.coef_idle = {948, 1115, 1282, 1613, 1654, 2167, 3182},
	},
};
static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
	.bw = {	160, 320, 480, 640, 800,
	960, 1280, 1600, 1920, 2240, 2560},
	},
	[DDR_600] = {
	.bw = {	192, 240, 480, 720, 960,
	1200, 1440, 1920, 2400, 2880, 3360},
	},
	[DDR_800] = {
	.bw = {	256, 320, 640, 960, 1280,
	1600, 1920, 2560, 3200, 3840, 4480},
	},
	[DDR_933] = {
	.bw = {	299, 373, 746, 1120, 1493,
	1866, 2239, 2986, 3732, 4478, 5225},
	},
	[DDR_1200] = {
	.bw = {	384, 480, 960, 1440, 1920,
	2400, 2880, 3840, 4800, 5760, 6720},
	},
	[DDR_1600] = {
	.bw = {	384, 512, 640, 1280, 1920,
	2560, 3200, 3840, 5120, 6400, 7680},
	},
	[DDR_2133] = {
	.bw = {	683, 853, 1706, 2560, 3413,
	4266, 5119, 6826, 8532, 10238, 11945},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {
	[APHY_VDDQ_0P6V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {140, 250, 420, 420, 540,
	600, 660, 760, 820, 820, 840},
			.write_coef = {270, 470, 700, 820, 1060,
	1140, 1390, 1650, 1830, 1970, 2150},
		},
		[DDR_600] = {
			.read_coef = {270, 300, 440, 640, 750,
	920, 930, 980, 1130, 1180, 1200},
			.write_coef = {450, 520, 890, 1190, 1340,
	1730, 1880, 2090, 2380, 2630, 2880},
		},
		[DDR_800] = {
			.read_coef = {280, 370, 520, 750, 940,
	1020, 1140, 1310, 1350, 1380, 1410},
			.write_coef = {500, 660, 930, 1310, 1610,
	1920, 2040, 2570, 2880, 3170, 3450},
		},
		[DDR_933] = {
			.read_coef = {300, 330, 660, 730, 880,
	1030, 1100, 1190, 1370, 1400, 1440},
			.write_coef = {510, 620, 1170, 1470, 1680,
	1990, 2380, 2590, 3070, 3370, 3740},
		},
		[DDR_1200] = {
			.read_coef = {230, 310, 480, 600, 800,
	930, 950, 1030, 1100, 1150, 1200},
			.write_coef = {480, 610, 920, 1410, 1760,
	2030, 2220, 2720, 3070, 3450, 3870},
		},
		[DDR_1600] = {
			.read_coef = {90, 110, 130, 190, 260,
	330, 380, 440, 470, 570, 640},
			.write_coef = {260, 340, 410, 740, 1070,
	1390, 1690, 1940, 2380, 2920, 3260},
		},
		[DDR_2133] = {
			.read_coef = {280, 280, 400, 460, 520,
	580, 620, 650, 730, 810, 900},
			.write_coef = {490, 530, 880, 1160, 1450,
	1710, 1870, 2310, 2720, 3150, 3560},
		},
	},
	.coef_idle = {110, 70, 70, 70, 70, 1310, 1340},
	},
	[APHY_VM_0P75V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {130, 220, 320, 380, 470,
	550, 680, 810, 910, 1020, 1090},
			.write_coef = {90, 140, 200, 240, 300,
	320, 390, 470, 510, 570, 610},
		},
		[DDR_600] = {
			.read_coef = {160, 190, 310, 450, 570,
	690, 780, 920, 1100, 1220, 1320},
			.write_coef = {140, 160, 260, 340, 380,
	490, 540, 600, 680, 760, 840},
		},
		[DDR_800] = {
			.read_coef = {180, 230, 370, 540, 690,
	790, 940, 1170, 1320, 1450, 1580},
			.write_coef = {160, 200, 280, 390, 480,
	580, 620, 780, 880, 960, 1050},
		},
		[DDR_933] = {
			.read_coef = {200, 230, 450, 590, 730,
	890, 1000, 1250, 1490, 1640, 1810},
			.write_coef = {170, 200, 370, 470, 530,
	630, 770, 820, 970, 1080, 1200},
		},
		[DDR_1200] = {
			.read_coef = {240, 310, 530, 730, 970,
	1150, 1210, 1580, 1810, 2020, 2240},
			.write_coef = {210, 270, 380, 600, 740,
	840, 900, 1100, 1200, 1330, 1480},
		},
		[DDR_1600] = {
			.read_coef = {315, 385, 465, 775, 1085,
	1325, 1505, 1705, 2045, 2405, 2595},
			.write_coef = {305, 365, 445, 715, 915,
	1075, 1075, 1295, 1475, 1665, 1795},
		},
		[DDR_2133] = {
			.read_coef = {996, 1103, 1426, 1646, 1866,
	2056, 2186, 2556, 2866, 3176, 3456},
			.write_coef = {946, 1021, 1246, 1386, 1526,
	1656, 1716, 1946, 2176, 2396, 2616},
		},
	},
	.coef_idle = {40, 40, 40, 40, 40, 45, 54},
	},
	[APHY_VIO_1P2V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {150, 270, 400, 520, 630,
	750, 930, 1120, 1200, 1380, 1400},
			.write_coef = {30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30},
		},
		[DDR_600] = {
			.read_coef = {130, 160, 320, 480, 610,
	770, 880, 1020, 1220, 1260, 1290},
			.write_coef = {20, 20, 20, 20, 20,
	20, 20, 20, 20, 20, 20},
		},
		[DDR_800] = {
			.read_coef = {130, 170, 330, 490, 640,
	730, 900, 1160, 1240, 1250, 1290},
			.write_coef = {30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30},
		},
		[DDR_933] = {
			.read_coef = {140, 180, 340, 490, 600,
	740, 820, 1050, 1250, 1280, 1330},
			.write_coef = {30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30},
		},
		[DDR_1200] = {
			.read_coef = {140, 180, 330, 480, 630,
	770, 800, 1110, 1210, 1270, 1270},
			.write_coef = {30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30},
		},
		[DDR_1600] = {
			.read_coef = {150, 190, 220, 400, 580,
	740, 850, 960, 1170, 1370, 1370},
			.write_coef = {60, 60, 60, 60, 60,
	60, 60, 60, 60, 60, 60},
		},
		[DDR_2133] = {
			.read_coef = {180, 210, 390, 560, 720,
	860, 870, 1150, 1320, 1360, 1587},
			.write_coef = {30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30},
		},
	},
	.coef_idle = {100, 100, 110, 130, 410, 430, 570},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf[] = {
	[DRAM_VDD1_1P8V] = {
		.i_dd0 = 5500,
		.i_dd2p = 650,
		.i_dd2n = 1750,
		.i_dd4r = 5900,
		.i_dd4w = 1750,
		.i_dd5 = 5250,
		.i_dd6 = 475,
	},
	[DRAM_VDD2_1P1V] = {
		.i_dd0 = 46000,
		.i_dd2p = 625,
		.i_dd2n = 9500,
		.i_dd4r = 143000,
		.i_dd4w = 176000,
		.i_dd5 = 21000,
		.i_dd6 = 625,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 5000,
		.i_dd2p = 2100,
		.i_dd2n = 3500,
		.i_dd4r = 34000,
		.i_dd4w = 6000,
		.i_dd5 = 4000,
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
			perf_event_release_kernel(l3_event);
			per_cpu(l3dc_events, cpu) = NULL;
		}
		if (i_event) {
			perf_event_release_kernel(i_event);
			per_cpu(inst_spec_events, cpu) = NULL;
		}
		if (c_event) {
			perf_event_release_kernel(c_event);
			per_cpu(cycle_events, cpu) = NULL;
		}
	}
}

static void swpm_pmu_set_enable_all(unsigned int enable)
{
	int i;

	if (enable) {
#ifdef CONFIG_MTK_CACHE_CONTROL
		ca_force_stop_set_in_kernel(1);
#endif
		for (i = 0; i < num_possible_cpus(); i++)
			swpm_pmu_set_enable(i, 1);
	} else {
		for (i = 0; i < num_possible_cpus(); i++)
			swpm_pmu_set_enable(i, 0);
#ifdef CONFIG_MTK_CACHE_CONTROL
		ca_force_stop_set_in_kernel(0);
#endif
	}
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

	/* init unsigned int ptr for share index iterator */
	idx_ref_uint_ptr = (unsigned int *)share_idx_ref;
	/* init output size to LTR except window_cnt with decrease 1 */
	idx_output_size =
		(sizeof(struct share_index)/sizeof(unsigned int)) - 1;


#if SWPM_TEST
	swpm_err("wrap_d = 0x%p, wrap index addr = 0x%p, wrap ctrl addr = 0x%p\n",
		 wrap_d, wrap_d->share_index_addr, wrap_d->share_ctrl_addr);
	swpm_err("share_idx_ref = 0x%p, share_idx_ctrl = 0x%p\n",
		 share_idx_ref, share_idx_ctrl);
	swpm_err("share_index size check = %d\n",
		 sizeof(struct share_index) / 4);
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
	int top_temp;

	if (!core_ptr)
		return;

	top_temp = get_immediate_tslvts3_2_wrap();

	/* truncate negative deg */
	top_temp = (top_temp < 0) ? 0 : top_temp;
	core_ptr->thermal = (unsigned int)top_temp;

#if SWPM_TEST
	swpm_err("swpm_core top lvts3_2 = %d\n", top_temp);
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
	//get gpu lkg and thermal
	if (swpm_info_ref) {
#ifdef CONFIG_THERMAL
		swpm_info_ref->gpu_reserved[gthermal + 1] =
			get_immediate_gpu_wrap() / 1000;
		swpm_info_ref->gpu_reserved[glkg + 1] =
			mt_gpufreq_get_leakage_no_lock();
#endif
	}

	swpm_core_thermal_cb();
}

static void swpm_idx_snap(void)
{
	unsigned long flags;

	if (share_idx_ref) {
		spin_lock_irqsave(&swpm_snap_spinlock, flags);
		/* directly copy due to 8 bytes alignment problem */
		mem_idx_snap.read_bw[0] = share_idx_ref->mem_idx.read_bw[0];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		spin_unlock_irqrestore(&swpm_snap_spinlock, flags);
	}
}

static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };
static char buf[POWER_CHAR_SIZE] = { 0 };

static void swpm_log_loop(struct timer_list *data)
{
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
//	trace_swpm_power(buf);

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
#endif /* #ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT : 545 */

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

	/* default 64 mA, efuse default mW to mA */
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

	/* default degree setting */
	core_ptr->thermal = CORE_DEFAULT_DEG;
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

	swpm_info("aphy others_bw[%ld]/others[%ld]/idd[%ld]/dram_opp[%d]\n",
		(unsigned long)sizeof(aphy_ref_others_bw_tbl),
		(unsigned long)sizeof(aphy_def_others_pwr_tbl),
		(unsigned long)sizeof(dram_def_pwr_conf),
		(unsigned short)sizeof(ddr_opp_freq));
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
	unsigned long flags;

	spin_lock_irqsave(&swpm_snap_spinlock, flags);
	seq_printf(m, "DRAM BW R/W=%d/%d\n",
		   mem_idx_snap.read_bw[0],
		   mem_idx_snap.write_bw[0]);
	spin_unlock_irqrestore(&swpm_snap_spinlock, flags);

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
		if (type >= NR_DRAM_PWR_TYPE ||
		    idd_idx >= (sizeof(struct dram_pwr_conf)
				/ sizeof(unsigned int)))
			goto end;
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
		swpm_set_update_cnt(0, (0x1 << 16 | pmu_ms_mode));
	} else
		swpm_err("echo <0/1> > /proc/swpm/pmu_ms_mode\n");

	return count;
}

static int core_static_replace_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "default: %d, replaced %d (valid:0~99)\n",
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
		core_lkg_replaced = (val < 100) ? val : core_lkg_replaced;

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
					swpm_pmu_set_enable_all(1);
				swpm_set_status(i);
			} else {
				if (!swpm_get_status(i))
					continue;

				if (i == CPU_POWER_METER)
					swpm_pmu_set_enable_all(0);
				swpm_clr_status(i);
			}
		}
		swpm_send_enable_ipi(type, enable);
	} else if (type < NR_POWER_METER) {
		if (enable && !swpm_get_status(type)) {
			if (type == CPU_POWER_METER)
				swpm_pmu_set_enable_all(1);
			swpm_set_status(type);
		} else if (!enable && swpm_get_status(type)) {
			if (type == CPU_POWER_METER)
				swpm_pmu_set_enable_all(0);
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
	case VDD12_DRAM:
		s = "VDD12_DRAM";
		break;
	case VDD18_DRAM:
		s = "VDD18_DRAM";
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

#if SWPM_TEST
	swpm_interface_unit_test();
#endif

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

}
module_exit(swpm_platform_exit)
