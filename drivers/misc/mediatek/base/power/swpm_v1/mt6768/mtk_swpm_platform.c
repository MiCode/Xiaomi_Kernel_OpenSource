/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <helio-dvfsrc-ipi.h>
#ifdef CONFIG_MTK_STATIC_POWER
#include <mtk_common_static_power.h>
#endif
#ifdef CONFIG_THERMAL
#include <mtk_thermal.h>
#endif
#ifdef CONFIG_MTK_CPU_FREQ
#include <mach/mtk_cpufreq_api.h>
#endif
#ifdef CONFIG_MTK_DRAMC
#include <mtk_dramc.h>
#endif
#include <mtk_swpm_common.h>
#include <mtk_swpm_platform.h>
#include <mtk_swpm.h>


/****************************************************************************
 *  Macro Definitions
 ****************************************************************************/

/****************************************************************************
 *  Type Definitions
 ****************************************************************************/

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
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

static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {800, 1200, 1600, 1800};

static struct aphy_pwr_data aphy_def_pwr_tbl[] = {
	[APHY_VCORE] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {227, 658, 1663, 2328, 4908},
				.coef = {2298, 3570, 5264, 5972, 8564},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {227, 658, 1664, 2330, 7659},
				.coef = {2698, 4354, 6387, 7263, 11574},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {329, 766, 1778, 2448, 8815},
				.coef = {3893, 6478, 9223, 10693, 17757},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {329, 766, 1778, 2448, 8815},
				.coef = {3893, 6478, 9223, 10693, 17757},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {227, 658, 1663, 2328, 4908},
				.coef = {2298, 3570, 5264, 5972, 8564},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {227, 658, 1664, 2330, 7659},
				.coef = {2698, 4354, 6387, 7263, 11574},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {329, 766, 1778, 2448, 8815},
				.coef = {3893, 6478, 9223, 10693, 17757},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {329, 766, 1778, 2448, 8815},
				.coef = {3893, 6478, 9223, 10693, 17757},
			},
		},
		.coef_idle = {2198, 2288, 3197, 3197},
	},
	[APHY_VDDQ_0P6V] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {148, 603, 1025, 1283, 1568},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {167, 680, 1175, 1352, 1703},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {98, 350, 673, 770, 988},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {98, 353, 667, 763, 1032},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {320, 640, 1280, 1920, 3200},
				.coef = {1225, 2188, 3300, 4327, 6127},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {480, 960, 1920, 2880, 4800},
				.coef = {1443, 2522, 3857, 5075, 7317},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {398, 2843, 3227, 5687, 7587},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {433, 1842, 3458, 6303, 8503},
			},
		},
		.coef_idle = {12, 12, 13, 13},
	},
	[APHY_VM_1P1V] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {107, 325, 491, 602, 674},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {166, 504, 821, 888, 1017},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {235, 663, 1095, 1221, 1388},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {248, 708, 1173, 1291, 1527},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {320, 640, 1280, 1920, 3200},
				.coef = {575, 958, 1376, 1673, 2332},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {480, 960, 1920, 2880, 4800},
				.coef = {867, 1415, 2011, 2434, 3800},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {356, 1200, 1905, 2686, 3255},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {377, 1223, 2045, 2891, 3645},
			},
		},
		.coef_idle = {55, 55, 55, 55},
	},
	[APHY_VIO_1P8V] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {30, 139, 268, 448, 543},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {98, 480, 960, 1920, 2880},
				.coef = {33, 153, 296, 457, 575},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {35, 158, 303, 452, 568},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {37, 162, 307, 444, 547},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 800 */
				.bw = {320, 640, 1280, 1920, 3200},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP1] = { /* freq = 1200 */
				.bw = {480, 960, 1920, 2880, 4800},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP2] = { /* freq = 1600 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP3] = { /* freq = 1800 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {0, 0, 0, 0, 0},
			},
		},
		.coef_idle = {99, 199, 209, 211},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf[] = {
	[DRAM_VDD1_1P8V] = {
		.i_dd0 = 10037,
		.i_dd2p = 293,
		.i_dd2n = 428,
		.i_dd4r = 14243,
		.i_dd4w = 15620,
		.i_dd5 = 1248,
		.i_dd6 = 218,
	},
	[DRAM_VDD2_1P1V] = {
		.i_dd0 = 37913,
		.i_dd2p = 314,
		.i_dd2n = 10530,
		.i_dd4r = 115136,
		.i_dd4w = 160981,
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

#ifdef CONFIG_MTK_DRAMC
static unsigned short ddr_opp_freq_lp3[NR_DDR_FREQ] = {600, 800, 933, 0};

static struct aphy_pwr_data aphy_def_pwr_tbl_lp3[] = {
	[APHY_VCORE] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {768, 1888, 2656, 3328, 5120},
				.coef = {6021, 8440, 9471, 10059, 12059},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {480, 768, 1920, 3328, 7872},
				.coef = {1840, 9612, 13202, 14567, 16670},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {13299, 15269, 19891, 22394, 31044},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {768, 1888, 2656, 3328, 5120},
				.coef = {6021, 8440, 9471, 10059, 12059},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {480, 768, 1920, 3328, 7872},
				.coef = {1840, 9612, 13202, 14567, 16670},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {768, 1152, 2208, 3360, 9216},
				.coef = {13299, 15269, 19891, 22394, 31044},
			},
		},
		.coef_idle = {4269, 4219, 6785},
	},
	[APHY_VDDQ_0P6V] = { /* VDDQ not used on LPDDR3 */
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {0, 0, 0, 0, 0},
				.coef = {0, 0, 0, 0, 0},
			},
		},
		.coef_idle = {0, 0, 0},
	},
	[APHY_VM_1P1V] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {107, 325, 491, 602, 674},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {0, 480, 960, 1920, 2880},
				.coef = {0, 656, 1132, 1227, 1376},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {235, 663, 1095, 1221, 1388},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {320, 640, 1280, 1920, 3200},
				.coef = {575, 958, 1376, 1673, 2332},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {480, 960, 1920, 2880, 4800},
				.coef = {1151, 1935, 2953, 3525, 4742},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {356, 1200, 1905, 2686, 3255},
			},
		},
		.coef_idle = {64, 64, 55},
	},
	[APHY_VIO_1P8V] = {
		.read_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {64, 320, 640, 1280, 1920},
				.coef = {30, 139, 268, 448, 543},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {0, 480, 960, 1920, 2880},
				.coef = {0, 151, 292, 461, 587},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {35, 158, 303, 452, 568},
			},
		},
		.write_pwr = {
			[DDR_OPP0] = { /* freq = 600 */
				.bw = {320, 640, 1280, 1920, 3200},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP1] = { /* freq = 800 */
				.bw = {480, 960, 1920, 2880, 4800},
				.coef = {0, 0, 0, 0, 0},
			},
			[DDR_OPP2] = { /* freq = 933 */
				.bw = {128, 640, 1280, 2560, 3840},
				.coef = {0, 0, 0, 0, 0},
			},
		},
		.coef_idle = {102, 175, 209},
	},
};

static struct dram_pwr_conf dram_def_pwr_conf_lp3[] = {
	[DRAM_VDD1_1P8V] = {
		.i_dd0 = 36200,
		.i_dd2p = 483,
		.i_dd2n = 4200,
		.i_dd4r = 8700,
		.i_dd4w = 8700,
		.i_dd5 = 6866,
		.i_dd6 = 483,
	},
	[DRAM_VDD2_1P1V] = {
		.i_dd0 = 40000,
		.i_dd2p = 446,
		.i_dd2n = 10000,
		.i_dd4r = 190500,
		.i_dd4w = 190500,
		.i_dd5 = 15000,
		.i_dd6 = 446,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 0,
		.i_dd2p = 0,
		.i_dd2n = 0,
		.i_dd4r = 0,
		.i_dd4w = 0,
		.i_dd5 = 0,
		.i_dd6 = 0,
	},
};

static unsigned int dram_type;
#endif

/****************************************************************************
 *  Global Variables
 ****************************************************************************/

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static void swpm_send_enable_ipi(unsigned int type, unsigned int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
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

#ifdef CONFIG_THERMAL
static unsigned int swpm_get_cpu_temp(enum cpu_lkg_type type)
{
	unsigned int temp;

	switch (type) {
	case CPU_L_LKG:
		temp = get_immediate_cpuL_wrap() / 1000;
		break;
	case CPU_B_LKG:
		temp = get_immediate_cpuB_wrap() / 1000;
		break;
	case DSU_LKG:
	default:
		temp = get_immediate_mcucci_wrap() / 1000;
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


/***************************************************************************
 *  API
 ***************************************************************************/
char *swpm_power_rail_to_string(enum power_rail p)
{
	char *s;

	switch (p) {
	case VPROC12:
		s = "VPROC12";
		break;
	case VPROC11:
		s = "VPROC11";
		break;
	case VGPU:
		s = "VGPU";
		break;
	case VCORE:
		s = "VCORE";
		break;
	case VDRAM1:
		s = "VDRAM1";
		break;
	case VDD1:
		s = "VDD1";
		break;
	default:
		s = "None";
		break;
	}

	return s;
}

int swpm_platform_init(void)
{
#ifdef CONFIG_MTK_DRAMC
	dram_type = get_ddr_type();

	/* copy pwr data */
	swpm_info_ref->ddr_type = dram_type;
	if (dram_type == TYPE_LPDDR3) {
		memcpy(swpm_info_ref->ddr_opp_freq, ddr_opp_freq_lp3,
			sizeof(ddr_opp_freq));
		memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl_lp3,
			sizeof(aphy_def_pwr_tbl));
		memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf_lp3,
			sizeof(dram_def_pwr_conf));

		swpm_info("copy pwr data (size: aphy/dram = %ld/%ld) done!\n",
			(unsigned long)sizeof(aphy_def_pwr_tbl_lp3),
			(unsigned long)sizeof(dram_def_pwr_conf_lp3));
	} else {
		memcpy(swpm_info_ref->ddr_opp_freq, ddr_opp_freq,
			sizeof(ddr_opp_freq));
		memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl,
			sizeof(aphy_def_pwr_tbl));
		memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf,
			sizeof(dram_def_pwr_conf));

		swpm_info("copy pwr data (size: aphy/dram = %ld/%ld) done!\n",
			(unsigned long)sizeof(aphy_def_pwr_tbl),
			(unsigned long)sizeof(dram_def_pwr_conf));
	}
#else
	memcpy(swpm_info_ref->ddr_opp_freq, ddr_opp_freq,
		sizeof(ddr_opp_freq));
	memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl,
		sizeof(aphy_def_pwr_tbl));
	memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf,
		sizeof(dram_def_pwr_conf));

	swpm_info("copy pwr data (size: aphy/dram = %ld/%ld) done!\n",
		(unsigned long)sizeof(aphy_def_pwr_tbl),
		(unsigned long)sizeof(dram_def_pwr_conf));
#endif

	return 0;
}

void swpm_send_init_ipi(unsigned int addr, unsigned int size,
	unsigned int ch_num)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_SWPM_INIT;
	qos_d.u.swpm_init.dram_addr = addr;
	qos_d.u.swpm_init.dram_size = size;
	qos_d.u.swpm_init.dram_ch_num = ch_num;
	qos_ipi_to_sspm_command(&qos_d, 4);
#endif
}

void swpm_set_enable(unsigned int type, unsigned int enable)
{
	if (type == 0xFFFF) {
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

void swpm_set_update_cnt(unsigned int type, unsigned int cnt)
{
	if (type != 0xFFFF && type >= NR_POWER_METER)
		return;

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	{
		struct qos_ipi_data qos_d;

		qos_d.cmd = QOS_IPI_SWPM_SET_UPDATE_CNT;
		qos_d.u.swpm_set_update_cnt.type = type;
		qos_d.u.swpm_set_update_cnt.cnt = cnt;
		qos_ipi_to_sspm_command(&qos_d, 3);
	}
#endif
}

void swpm_update_lkg_table(void)
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
}

