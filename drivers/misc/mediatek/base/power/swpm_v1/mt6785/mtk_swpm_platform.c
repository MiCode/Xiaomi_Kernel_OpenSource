/*
 * Copyright (C) 2017 MediaTek Inc.
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

static unsigned short ddr_opp_freq[NR_DDR_FREQ] = {
	400, 600, 800, 1200, 1600, 1866, 2133
};

static struct aphy_pwr_data aphy_def_pwr_tbl[] = {
	[APHY_VCORE] = {
		.pwr = {
			[DDR_400] = {
				.bw = {330, 480, 640, 800, 960, 1280,
					1600, 1920, 2240, 2560},
				.coef = {1639, 3504, 4144, 4704, 5184,
					6192, 7104, 7904, 8544, 8624},
			},
			[DDR_600] = {
				.bw = {240, 480, 720, 960, 1200, 1920,
					2400, 2880, 3360, 4320},
				.coef = {4480, 8528, 9952, 10912, 11632,
					13840, 15152, 16112, 16912, 17792},
			},
			[DDR_800] = {
				.bw = {320, 640, 960, 1280, 1600, 1920,
					2560, 3200, 4480, 5760},
				.coef = {3872, 5792, 6752, 7872, 8592,
					9312, 10592, 11872, 13312, 13472},
			},
			[DDR_1200] = {
				.bw = {288, 480, 960, 1920, 2880, 3840,
					4800, 5760, 6720, 8640},
				.coef = {8608, 10528, 12128, 16608, 19168,
					20128, 21888, 23648, 24928, 25888},
			},
			[DDR_1600] = {
				.bw = {384, 640, 1920, 2560, 3200, 3840,
					5120, 6400, 8960, 11520},
				.coef = {12043, 14043, 19543, 21257, 22900,
					24329, 26757, 28757, 31043, 32043},
			},
			[DDR_1866] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {11712, 14650, 17088, 21338, 23838,
					27963, 29963, 35163, 36713, 38588},
			},
			[DDR_2133] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {13388, 19533, 24391, 27248, 31964,
					34250, 37393, 40194, 41966, 44109},
			},

		},
		.coef_idle = {768, 1536, 2816, 4195, 5359, 6264, 7160},
	},
	[APHY_VDDQ_0P6V] = {
		.pwr = {
			[DDR_400] = {
				.bw = {330, 480, 640, 800, 960, 1280,
					1600, 1920, 2240, 2560},
				.coef = {877, 1281, 1530, 1812, 2037,
					2453, 2837, 3187, 3470, 3703},
			},
			[DDR_600] = {
				.bw = {240, 480, 720, 960, 1200, 1920,
					2400, 2880, 3360, 4320},
				.coef = {465, 1357, 1887, 2240, 2607,
					3523, 4007, 4440, 4832, 5392},
			},
			[DDR_800] = {
				.bw = {256, 640, 960, 1280, 1600, 1920,
					2560, 3200, 4480, 5760},
				.coef = {722, 1777, 2357, 2865, 3298,
					3648, 4190, 4707, 5648, 6248},
			},
			[DDR_1200] = {
				.bw = {288, 480, 960, 1920, 2880, 3840,
					4800, 5760, 6720, 8640},
				.coef = {397, 632, 882, 2107, 2990,
					3373, 4057, 4673, 5240, 6340},
			},
			[DDR_1600] = {
				.bw = {384, 640, 1920, 2560, 3200, 3840,
					5120, 6400, 8960, 11520},
				.coef = {318, 507, 1197, 1630, 1988,
					2363, 2697, 3297, 4313, 4947},
			},
			[DDR_1866] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {385, 852, 1402, 1877, 2635,
					3002, 3602, 4118, 4525, 5212},
			},
			[DDR_2133] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {385, 852, 1402, 1877, 2635,
					3002, 3602, 4118, 4525, 5212},
			},
		},
		.coef_idle = {13, 10, 18, 10, 37, 32, 32},
	},
	[APHY_VM_0P75V] = {
		.pwr = {
			[DDR_400] = {
				.bw = {320, 480, 640, 800, 960, 1280,
					1600, 1920, 2240, 2560},
				.coef = {372, 552, 687, 825, 959,
					1212, 1439, 1679, 1892, 2019},
			},
			[DDR_600] = {
				.bw = {240, 480, 720, 960, 1200, 1920,
					2400, 2880, 3360, 4320},
				.coef = {185, 516, 735, 912, 1089,
					1579, 1872, 2152, 2415, 2619},
			},
			[DDR_800] = {
				.bw = {256, 640, 960, 1280, 1600, 1920,
					2560, 3200, 4480, 5760},
				.coef = {281, 709, 964, 1212, 1439,
					1644, 2001, 2352, 2951, 3137},
			},
			[DDR_1200] = {
				.bw = {288, 480, 960, 1920, 2880, 3840,
					4800, 5760, 6720, 8640},
				.coef = {276, 420, 572, 1353, 1945,
					2207, 2580, 3167, 3553, 4129},
			},
			[DDR_1600] = {
				.bw = {384, 640, 1920, 2560, 3200, 3840,
					5120, 6400, 8960, 11520},
				.coef = {553, 735, 1593, 1913, 2197,
					2471, 2953, 3393, 4001, 4256},
			},
			[DDR_1866] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {449, 893, 1441, 1897, 2564,
					2897, 3412, 3897, 4257, 4871},
			},
			[DDR_2133] = {
				.bw = {448, 1493, 2239, 2986, 4478, 5971,
					7464, 8957, 10450, 13435},
				.coef = {449, 893, 1441, 1897, 2564,
					2897, 3412, 3897, 4257, 4871},
			},
		},
		.coef_idle = {95, 101, 103, 100, 100, 103, 103},
	},
	[APHY_VIO_1P2V] = {
		.pwr = {
			[DDR_400] = {
				.bw = {320, 480, 640, 800, 960, 1280,
					1600, 1920, 2240, 2560},
				.coef = {138, 209, 263, 318, 373,
					476, 573, 677, 768, 813},
			},
			[DDR_600] = {
				.bw = {240, 480, 720, 960, 1200, 1920,
					2400, 2880, 3360, 3840},
				.coef = {51, 153, 223, 279, 338,
					500, 605, 704, 798, 826},
			},
			[DDR_800] = {
				.bw = {256, 640, 960, 1280, 1600, 1920,
					2560, 3200, 3840, 4480},
				.coef = {52, 178, 251, 325, 394,
					461, 582, 704, 819, 882},
			},
			[DDR_1200] = {
				.bw = {288, 480, 960, 1440, 1920, 2880,
					3840, 4800, 5760, 6720},
				.coef = {52, 82, 115, 212, 283,
					412, 475, 598, 725, 828},
			},
			[DDR_1600] = {
				.bw = {384, 640, 1280, 1920, 2560, 3200,
					3840, 5120, 6400, 8960},
				.coef = {94, 128, 219, 294, 364,
					428, 498, 623, 750, 836},
			},
			[DDR_1866] = {
				.bw = {448, 1493, 2239, 2986, 3732, 4478,
					5971, 7464, 8957, 10450},
				.coef = {61, 128, 219, 298, 362,
					428, 501, 619, 747, 816},
			},
			[DDR_2133] = {
				.bw = {448, 1493, 2239, 2986, 3732, 4478,
					5971, 7464, 8957, 10450},
				.coef = {61, 128, 219, 298, 362,
					428, 501, 619, 747, 816},
			},
		},
		.coef_idle = {91, 71, 109, 288, 323, 351, 351},
	},
	[APHY_VIO_1P8V] = {
		.pwr = {
			[DDR_400] = {
				.bw = {320, 480, 640, 800, 960, 1280,
					1600, 1920, 2240, 2560},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_600] = {
				.bw = {240, 480, 720, 960, 1200, 1920,
					2400, 2880, 3360, 3840},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_800] = {
				.bw = {256, 640, 960, 1280, 1600, 1920,
					2560, 3200, 3840, 4480},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_1200] = {
				.bw = {288, 480, 960, 1440, 1920, 2880,
					3840, 4800, 5760, 6720},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_1600] = {
				.bw = {384, 640, 1280, 1920, 2560, 3200,
					3840, 5120, 6400, 8960},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_1866] = {
				.bw = {448, 1493, 2239, 2986, 3732, 4478,
					5971, 7464, 8957, 10450},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
			[DDR_2133] = {
				.bw = {448, 1493, 2239, 2986, 3732, 4478,
					5971, 7464, 8957, 10450},
				.coef = {14, 14, 14, 14, 14,
					14, 14, 14, 14, 14},
			},
		},
		.coef_idle = {2, 2, 2, 14, 14, 14, 14},
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
		.i_dd0 = 50000,
		.i_dd2p = 314,
		.i_dd2n = 8000,
		.i_dd4r = 150000,
		.i_dd4w = 180000,
		.i_dd5 = 12040,
		.i_dd6 = 266,
	},
	[DRAM_VDDQ_0P6V] = {
		.i_dd0 = 140,
		.i_dd2p = 75,
		.i_dd2n = 75,
		.i_dd4r = 230000,
		.i_dd4w = 511,
		.i_dd5 = 75,
		.i_dd6 = 36,
	},
};

/****************************************************************************
 *  Global Variables
 ****************************************************************************/

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
	case VIO12_DDR:
		s = "VIO12_DDR";
		break;
	case VIO18_DDR:
		s = "VIO18_DDR";
		break;
	case VIO18_DRAM:
		s = "VIO18_DRAM";
		break;
	case VVPU:
		s = "VVPU";
		break;
	default:
		s = "None";
		break;
	}

	return s;
}

int swpm_platform_init(void)
{
	/* copy pwr data */
	memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl,
		sizeof(aphy_def_pwr_tbl));
	memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf,
		sizeof(dram_def_pwr_conf));
	memcpy(swpm_info_ref->ddr_opp_freq, ddr_opp_freq,
		sizeof(ddr_opp_freq));

	swpm_info("copy pwr data (size: aphy/dram = %ld/%ld) done!\n",
		(unsigned long)sizeof(aphy_def_pwr_tbl),
		(unsigned long)sizeof(dram_def_pwr_conf));

	return 0;
}

void swpm_send_init_ipi(unsigned int addr, unsigned int size,
	unsigned int ch_num)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) &&		\
	defined(CONFIG_MTK_QOS_FRAMEWORK)
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

void swpm_update_gpu_counter(unsigned int gpu_pmu[])
{
	memcpy(swpm_info_ref->gpu_counter, gpu_pmu,
		sizeof(swpm_info_ref->gpu_counter));
}

int swpm_get_gpu_enable(void)
{
	if (!swpm_info_ref)
		return 0;

	return swpm_info_ref->gpu_enable;
}

