/* SPDX-License-Identifier: GPL-2.0 */
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
#include <mtk_swpm_sp_platform.h>
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

struct core_swpm_rec_data *core_ptr;
struct mem_swpm_rec_data *mem_ptr;

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

static unsigned short core_volt_tbl[NR_CORE_VOLT] = {
	575, 600, 650, 725,
};
static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 933, 1200, 1600, 1866,
};
static struct aphy_core_bw_data aphy_ref_core_bw_tbl[] = {
	[DDR_400] = {
		.bw = {320, 640, 960, 1280, 1600, 1920,
		2560, 3200, 3840, 4480, 5120},
	},
	[DDR_600] = {
		.bw = {384, 480, 960, 1440, 1920, 2400,
		2880, 3840, 4800, 5760, 6720},
	},
	[DDR_800] = {
		.bw = {384, 512, 640, 1280, 1920, 2560,
		3200, 3840, 5120, 6400, 7680},
	},
	[DDR_933] = {
		.bw = {448, 597, 746, 1493, 2239, 2986,
		3732, 4478, 5971, 7464, 8957},
	},
	[DDR_1200] = {
		.bw = {576, 768, 960, 1920, 2880, 3840,
		4800, 5760, 7680, 9600, 11520},
	},
	[DDR_1600] = {
		.bw = {512, 768, 1024, 1280, 2560, 3840,
		5120, 6400, 7680, 10240, 12800},
	},
	[DDR_1866] = {
		.bw = {597, 896, 1195, 1493, 2986, 4480,
		5973, 7466, 8959, 11946, 14932},
	},
};
static struct aphy_core_pwr_data aphy_def_core_pwr_tbl[] = {
	[APHY_VCORE] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {9618, 10117, 10444, 10541, 10982, 11079,
	11214, 11408, 11543, 11622, 11815},
			.write_coef = {9503, 9772, 9927, 9966, 10120, 10044,
	9892, 9970, 9761, 9667, 9342},
		},
		[DDR_600] = {
			.read_coef = {9674, 9913, 10536, 11044, 11379, 11656,
	11991, 12374, 12871, 13197, 13407},
			.write_coef = {9501, 9798, 10134, 10526, 10631, 10909,
	11014, 11051, 11146, 11012, 11279},
		},
		[DDR_800] = {
			.read_coef = {10141, 10538, 10705, 11598, 12146, 12578,
	12896, 13157, 13793, 14429, 15007},
			.write_coef = {10083, 10480, 10417, 11195, 11513, 11716,
	11861, 11892, 12355, 12186, 12304},
		},
		[DDR_933] = {
			.read_coef = {10788, 11232, 11274, 12460, 13071, 13337,
	14178, 14444, 15148, 15795, 16385},
			.write_coef = {10673, 10944, 11101, 11885, 12093, 12589,
	12913, 12891, 13136, 13150, 13452},
		},
		[DDR_1200] = {
			.read_coef = {11801, 12297, 12553, 14193, 14873, 15733,
	16113, 16673, 17432, 18252, 19072},
			.write_coef = {11561, 12237, 12373, 13473, 14033, 14533,
	14793, 15053, 15152, 15252, 15472},
		},
		[DDR_1600] = {
			.read_coef = {14765, 15660, 16231, 16866, 19200, 20688,
	21851, 22494, 23527, 25333, 26815},
			.write_coef = {14375, 15075, 15971, 16411, 18030, 18673,
	19121, 19764, 19822, 20458, 20510},
		},
		[DDR_1866] = {
			.read_coef = {21626, 22784, 25174, 24954, 28639, 30367,
	32965, 33823, 35188, 37484, 39997},
			.write_coef = {21264, 22349, 24304, 24809, 26827, 27902,
	28615, 29255, 29605, 29654, 29557},
		},
	},
	.coef_idle = {0, 0, 0, 0, 0, 0, 0},
	},
};
static struct aphy_others_bw_data aphy_ref_others_bw_tbl[] = {
	[DDR_400] = {
		.bw = {320, 640, 960, 1280, 1600, 1920,
		2560, 3200, 3840, 4480, 5120},
	},
	[DDR_600] = {
		.bw = {384, 480, 960, 1440, 1920, 2400,
		2880, 3840, 4800, 5760, 6720},
	},
	[DDR_800] = {
		.bw = {384, 512, 640, 1280, 1920, 2560,
		3200, 3840, 5120, 6400, 7680},
	},
	[DDR_933] = {
		.bw = {448, 597, 746, 1493, 2239, 2986,
		3732, 4478, 5971, 7464, 8957},
	},
	[DDR_1200] = {
		.bw = {576, 768, 960, 1920, 2880, 3840,
		4800, 5760, 7680, 9600, 11520},
	},
	[DDR_1600] = {
		.bw = {512, 768, 1024, 1280, 2560, 3840,
		5120, 6400, 7680, 10240, 12800},
	},
	[DDR_1866] = {
		.bw = {597, 896, 1195, 1493, 2986, 4480,
		5973, 7466, 8959, 11946, 14932},
	},
};
static struct aphy_others_pwr_data aphy_def_others_pwr_tbl[] = {
	[APHY_VDDQ_0P6V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {683, 1017, 1283, 1467, 1800, 2000,
	2483, 2883, 3300, 3733, 4083},
			.write_coef = {717, 1067, 1367, 1583, 1917, 2133,
	2617, 3100, 3517, 3950, 4300},
		},
		[DDR_600] = {
			.read_coef = {850, 933, 1383, 1850, 2283, 2683,
	3050, 3650, 4300, 4867, 5417},
			.write_coef = {883, 1017, 1517, 2000, 2467, 2900,
	3300, 3950, 4617, 5183, 5733},
		},
		[DDR_800] = {
			.read_coef = {867, 1000, 1167, 1783, 2350, 2883,
	3383, 3850, 4700, 5533, 6167},
			.write_coef = {917, 1083, 1250, 1950, 2567, 3133,
	3683, 4033, 5100, 5933, 6733},
		},
		[DDR_933] = {
			.read_coef = {1050, 1233, 1383, 2167, 2833, 3383,
	4117, 4517, 5600, 6467, 7417},
			.write_coef = {1117, 1317, 1483, 2283, 2967, 3750,
	4400, 4900, 5983, 6917, 7883},
		},
		[DDR_1200] = {
			.read_coef = {1200, 1433, 1717, 2700, 3517, 4383,
	5083, 5833, 7033, 8183, 9350},
			.write_coef = {1217, 1500, 1683, 2717, 3517, 4517,
	5200, 5850, 7133, 8367, 9433},
		},
		[DDR_1600] = {
			.read_coef = {5133, 5650, 6133, 6633, 8967, 11233,
	13350, 15417, 17500, 21767, 25933},
			.write_coef = {4700, 5017, 5333, 5600, 6983, 8133,
	9300, 10433, 11433, 13567, 15467},
		},
		[DDR_1866] = {
			.read_coef = {5550, 6100, 6867, 7250, 9717, 11950,
	14267, 16150, 18333, 22333, 26333},
			.write_coef = {6100, 6417, 7783, 8967, 10100, 11150,
	12217, 14167, 16033, 17667, 19833},
		},
	},
	.coef_idle = {0, 0, 0, 0, 0, 0, 0},
	},
	[APHY_VM_0P75V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {428, 676, 897, 1090, 1338, 1517,
	1903, 2221, 2538, 2897, 3172},
			.write_coef = {317, 469, 593, 690, 828, 910,
	1103, 1283, 1448, 1614, 1738},
		},
		[DDR_600] = {
			.read_coef = {497, 566, 897, 1241, 1559, 1848,
	2124, 2524, 2979, 3379, 3793},
			.write_coef = {414, 455, 676, 883, 1090, 1269,
	1421, 1683, 1945, 2166, 2372},
		},
		[DDR_800] = {
			.read_coef = {497, 593, 690, 1117, 1503, 1890,
	2221, 2538, 3131, 3379, 4207},
			.write_coef = {428, 455, 579, 869, 1145, 1393,
	1614, 1752, 2207, 2524, 2828},
		},
		[DDR_933] = {
			.read_coef = {593, 717, 814, 1324, 1779, 2166,
	2648, 2924, 3655, 4207, 4897},
			.write_coef = {524, 607, 690, 1048, 1338, 1697,
	1972, 2179, 2621, 2993, 3393},
		},
		[DDR_1200] = {
			.read_coef = {662, 814, 952, 1586, 2124, 2676,
	3117, 3600, 4345, 5145, 5917},
			.write_coef = {579, 717, 800, 1283, 1641, 2097,
	2414, 2703, 3241, 3752, 4193},
		},
		[DDR_1600] = {
			.read_coef = {717, 938, 1131, 1324, 2166, 2966,
	3628, 4124, 4703, 5793, 6800},
			.write_coef = {634, 800, 979, 1131, 1807, 2303,
	2786, 3200, 3490, 4248, 4828},
		},
		[DDR_1866] = {
			.read_coef = {1310, 1600, 2152, 2207, 3310, 4055,
	4979, 5448, 6041, 7062, 8138},
			.write_coef = {2041, 2221, 2966, 3434, 3848, 4290,
	4634, 5214, 5821, 6345, 7090},
		},
	},
	.coef_idle = {0, 0, 0, 0, 0, 0, 0},
	},
	[APHY_VIO_1P2V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {500, 675, 858, 1025, 1200, 1333,
	1650, 1867, 2058, 2325, 2533},
			.write_coef = {317, 325, 325, 325, 325, 325,
	325, 325, 325, 325, 325},
		},
		[DDR_600] = {
			.read_coef = {442, 492, 717, 933, 1150, 1350,
	1525, 1700, 1958, 2175, 2392},
			.write_coef = {275, 275, 275, 275, 275, 275,
	275, 275, 275, 275, 275},
		},
		[DDR_800] = {
			.read_coef = {425, 458, 550, 783, 992, 1208,
	1358, 1517, 1800, 2225, 2250},
			.write_coef = {317, 317, 342, 342, 342, 342,
	342, 342, 342, 342, 342},
		},
		[DDR_933] = {
			.read_coef = {508, 558, 600, 833, 1050, 1217,
	1458, 1550, 1833, 2017, 2292},
			.write_coef = {367, 375, 392, 392, 392, 392,
	392, 392, 392, 392, 392},
		},
		[DDR_1200] = {
			.read_coef = {825, 875, 917, 1150, 1367, 1575,
	1750, 1925, 2108, 2350, 2617},
			.write_coef = {683, 692, 717, 717, 717, 717,
	717, 717, 717, 717, 717},
		},
		[DDR_1600] = {
			.read_coef = {867, 933, 983, 1042, 1325, 1600,
	1825, 1975, 2117, 2350, 2650},
			.write_coef = {750, 758, 758, 783, 783, 783,
	783, 783, 783, 783, 783},
		},
		[DDR_1866] = {
			.read_coef = {1025, 1083, 1150, 1200, 1475, 1742,
	2008, 2125, 2292, 2508, 2783},
			.write_coef = {925, 925, 925, 925, 925, 925,
	925, 925, 925, 925, 925},
		},
	},
	.coef_idle = {0, 0, 0, 0, 0, 0, 0},
	},
	[APHY_VIO_1P8V] = {
	.pwr = {
		[DDR_400] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_600] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_800] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_933] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_1200] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_1600] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
		[DDR_1866] = {
			.read_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
			.write_coef = {3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3},
		},
	},
	.coef_idle = {0, 0, 0, 0, 0, 0, 0},
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
		/* SW co-relation workaround not used */
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

	/* init extension index address */
	swpm_sp_init(sspm_sbuf_get(wrap_d->share_index_ext_addr),
		     sspm_sbuf_get(wrap_d->share_ctrl_ext_addr));

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
			lkg = mt_spower_get_leakage(dev_id, volt, temp) * 1000;
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
}

static void swpm_idx_snap(void)
{
	unsigned long flags;

	if (share_idx_ref) {
		spin_lock_irqsave(&swpm_snap_spinlock, flags);
		/* directly copy due to 8 bytes alignment problem */
		mem_idx_snap.read_bw[0] = share_idx_ref->mem_idx.read_bw[0];
		mem_idx_snap.read_bw[1] = share_idx_ref->mem_idx.read_bw[1];
		mem_idx_snap.write_bw[0] = share_idx_ref->mem_idx.write_bw[0];
		mem_idx_snap.write_bw[1] = share_idx_ref->mem_idx.write_bw[1];
		spin_unlock_irqrestore(&swpm_snap_spinlock, flags);
	}
}

static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };
static char buf[POWER_CHAR_SIZE] = { 0 };

static void swpm_log_loop(unsigned long data)
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

	swpm_core_pwr_data_init();
	swpm_mem_pwr_data_init();
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
	seq_printf(m, "DRAM BW [N]R/W=%d/%d,[S]R/W=%d/%d\n",
		   mem_idx_snap.read_bw[0],
		   mem_idx_snap.write_bw[0],
		   mem_idx_snap.read_bw[1],
		   mem_idx_snap.write_bw[1]);
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

PROC_FOPS_RW(idd_tbl);
PROC_FOPS_RO(dram_bw);
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

	swpm_append_procfs(&idd_tbl);
	swpm_append_procfs(&dram_bw);
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
	swpm_sp_exit();
}
module_exit(swpm_platform_exit)

