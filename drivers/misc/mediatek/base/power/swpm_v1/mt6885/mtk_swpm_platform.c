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
#include <sspm_reservedmem.h>
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
	400, 600, 800, 933, 1200, 1600, 1866,
};

static struct aphy_pwr_data aphy_def_pwr_tbl[] = {
	[APHY_VCORE] = {
		.pwr = {
		[DDR_400] = {
			.bw = {142, 300, 566, 1300, 1929,
				2566, 3176, 3847, 4471, 5296},
			.coef = {2812, 2934, 3139, 3675, 4118,
				4550, 5030, 5458, 5640, 5637},
		},
		[DDR_600] = {
			.bw = {213, 797, 2026, 3859, 4493,
				5042, 6171, 6701, 7262, 7892},
			.coef = {2876, 3435, 4544, 6270, 6610,
				7064, 7068, 7066, 7061, 7047},
		},
		[DDR_800] = {
			.bw = {283, 1077, 2697, 5145, 5990,
				6738, 8266, 8899, 9650, 10442},
			.coef = {4455, 6263, 8250, 10919, 11576,
				11858, 11860, 11849, 11860, 11819},
		},
		[DDR_933] = {
			.bw = {345, 1253, 3150, 6005, 6995,
				7859, 9607, 10375, 11222, 12230},
			.coef = {5014, 7050, 9338, 12400, 13161,
				13807, 13818, 13834, 13791, 13787},
		},
		[DDR_1200] = {
			.bw = {427, 1621, 4039, 7716, 8999,
				10061, 12290, 13249, 14410, 15652},
			.coef = {5385, 8146, 11182, 14997, 16049,
				16419, 16388, 16384, 16406, 16449},
		},
		[DDR_1600] = {
			.bw = {583, 2218, 5530, 10540, 12232,
				13670, 16677, 17946, 19335, 21059},
			.coef = {6900, 10605, 14539, 19275, 20577,
				21633, 21661, 21682, 21609, 21627},
		},
		[DDR_1866] = {
			.bw = {687, 2587, 6487, 12243, 14089,
				15709, 19146, 20456, 22624, 24214},
			.coef = {8153, 12611, 17655, 22792, 24193,
				25444, 25504, 25338, 25769, 25409},
		},
		},
		.coef_idle = {3602, 3561, 4873, 5377, 5512, 6667, 7597},
	},
	[APHY_VDDQ_0P6V] = {
		.pwr = {
		[DDR_400] = {
			.bw = {142, 300, 566, 1300, 1929,
				2566, 3176, 3847, 4471, 5296},
			.coef = {518, 869, 1580, 3037, 4210,
				5349, 6345, 7213, 7582, 7574},
		},
		[DDR_600] = {
			.bw = {213, 797, 2026, 3859, 4493,
				5042, 6171, 6701, 7262, 7892},
			.coef = {845, 2949, 5790, 9788, 10461,
				11359, 11369, 11364, 11354, 11326},
		},
		[DDR_800] = {
			.bw = {283, 1077, 2697, 5145, 5990,
				6738, 8266, 8899, 9650, 10442},
			.coef = {1206, 4031, 7707, 12995, 14441,
				15061, 15066, 15041, 15066, 14976},
		},
		[DDR_933] = {
			.bw = {345, 1253, 3150, 6005, 6995,
				7859, 9607, 10375, 11222, 12230},
			.coef = {1438, 4521, 8592, 14397, 16000,
				17363, 17388, 17421, 17330, 17322},
		},
		[DDR_1200] = {
			.bw = {427, 1621, 4039, 7716, 8999,
				10061, 12290, 13249, 14410, 15652},
			.coef = {1753, 5759, 11195, 18571, 20807,
				21593, 21528, 21519, 21565, 21657},
		},
		[DDR_1600] = {
			.bw = {583, 2218, 5530, 10540, 12232,
				13670, 16677, 17946, 19335, 21059},
			.coef = {3763, 11575, 21579, 35022, 39018,
				42259, 42343, 42407, 42185, 42240},
		},
		[DDR_1866] = {
			.bw = {687, 2587, 6487, 12243, 14089,
				15709, 19146, 20456, 22624, 24214},
			.coef = {4087, 12378, 23514, 36822, 40709,
				44116, 44287, 43929, 44767, 44014},
		},
		},
		.coef_idle = {21, 21, 21, 21, 21, 21, 21},
	},
	[APHY_VM_0P75V] = {
		.pwr = {
		[DDR_400] = {
			.bw = {142, 300, 566, 1300, 1929,
				2566, 3176, 3847, 4471, 5296},
			.coef = {421, 583, 909, 1582, 2124,
				2651, 3115, 3520, 3693, 3689},
		},
		[DDR_600] = {
			.bw = {213, 797, 2026, 3859, 4493,
				5042, 6171, 6701, 7262, 7892},
			.coef = {572, 1537, 2849, 4698, 5011,
				5428, 5432, 5430, 5426, 5413},
		},
		[DDR_800] = {
			.bw = {283, 1077, 2697, 5145, 5990,
				6738, 8266, 8899, 9650, 10442},
			.coef = {740, 2044, 3749, 6206, 6880,
				7169, 7171, 7159, 7171, 7129},
		},
		[DDR_933] = {
			.bw = {345, 1253, 3150, 6005, 6995,
				7859, 9607, 10375, 11222, 12230},
			.coef = {844, 2260, 4137, 6817, 7559,
				8190, 8202, 8217, 8175, 8171},
		},
		[DDR_1200] = {
			.bw = {427, 1621, 4039, 7716, 8999,
				10061, 12290, 13249, 14410, 15652},
			.coef = {1017, 2923, 5518, 9045, 10116,
				10493, 10461, 10457, 10479, 10523},
		},
		[DDR_1600] = {
			.bw = {583, 2218, 5530, 10540, 12232,
				13670, 16677, 17946, 19335, 21059},
			.coef = {1367, 3819, 6969, 11207, 12468,
				13491, 13517, 13538, 13468, 13485},
		},
		[DDR_1866] = {
			.bw = {687, 2587, 6487, 12243, 14089,
				15709, 19146, 20456, 22624, 24214},
			.coef = {1628, 4539, 8462, 13154, 14525,
				15728, 15788, 15661, 15959, 15692},
		},
		},
		.coef_idle = {256, 256, 256, 256, 256, 256, 256},
	},
	[APHY_VIO_1P2V] = {
		.pwr = {
		[DDR_400] = {
			.bw = {142, 300, 566, 1300, 1929,
				2566, 3176, 3847, 4471, 5296},
			.coef = {175, 175, 175, 175, 175,
				175, 175, 175, 175, 175},
		},
		[DDR_600] = {
			.bw = {213, 797, 2026, 3859, 4493,
				5042, 6171, 6701, 7262, 7892},
			.coef = {115, 115, 115, 115, 115,
				115, 115, 115, 115, 115},
		},
		[DDR_800] = {
			.bw = {283, 1077, 2697, 5145, 5990,
				6738, 8266, 8899, 9650, 10442},
			.coef = {175, 175, 175, 175, 175,
				175, 175, 175, 175, 175},
		},
		[DDR_933] = {
			.bw = {345, 1253, 3150, 6005, 6995,
				7859, 9607, 10375, 11222, 12230},
			.coef = {213, 213, 213, 213, 213,
				213, 213, 213, 213, 213},
		},
		[DDR_1200] = {
			.bw = {427, 1621, 4039, 7716, 8999,
				10061, 12290, 13249, 14410, 15652},
			.coef = {565, 565, 565, 565, 565,
				565, 565, 565, 565, 565},
		},
		[DDR_1600] = {
			.bw = {583, 2218, 5530, 10540, 12232,
				13670, 16677, 17946, 19335, 21059},
			.coef = {685, 685, 685, 685, 685,
				685, 685, 685, 685, 685},
		},
		[DDR_1866] = {
			.bw = {687, 2587, 6487, 12243, 14089,
				15709, 19146, 20456, 22624, 24214},
			.coef = {771, 771, 771, 771, 771,
				771, 771, 771, 771, 771},
		},
		},
		.coef_idle = {233, 153, 233, 283, 753, 913, 1028},
	},
	[APHY_VIO_1P8V] = {
		.pwr = {
		[DDR_400] = {
			.bw = {142, 300, 566, 1300, 1929,
				2566, 3176, 3847, 4471, 5296},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_600] = {
			.bw = {213, 797, 2026, 3859, 4493,
				5042, 6171, 6701, 7262, 7892},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_800] = {
			.bw = {283, 1077, 2697, 5145, 5990,
				6738, 8266, 8899, 9650, 10442},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_933] = {
			.bw = {345, 1253, 3150, 6005, 6995,
				7859, 9607, 10375, 11222, 12230},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_1200] = {
			.bw = {427, 1621, 4039, 7716, 8999,
				10061, 12290, 13249, 14410, 15652},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_1600] = {
			.bw = {583, 2218, 5530, 10540, 12232,
				13670, 16677, 17946, 19335, 21059},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		[DDR_1866] = {
			.bw = {687, 2587, 6487, 12243, 14089,
				15709, 19146, 20456, 22624, 24214},
			.coef = {0, 0, 0, 0, 0,
				0, 0, 0, 0, 0},
		},
		},
		.coef_idle = {3, 3, 3, 3, 3, 3, 3},
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

/* subsys share mem reference table */
static struct swpm_mem_ref_tbl mem_ref_tbl[NR_POWER_METER] = {
	[CPU_POWER_METER] = {0, NULL},
	[GPU_POWER_METER] = {0, NULL},
	[CORE_POWER_METER] = {0, NULL},
	[MEM_POWER_METER] = {0, NULL},
	[ISP_POWER_METER] = {0, NULL},
};

static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };
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

static void swpm_send_init_ipi(unsigned int addr, unsigned int size,
			      unsigned int ch_num)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && \
	defined(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;
	struct share_wrap *wrap_d;
	unsigned int offset;

	qos_d.cmd = QOS_IPI_SWPM_INIT;
	qos_d.u.swpm_init.dram_addr = addr;
	qos_d.u.swpm_init.dram_size = size;
	qos_d.u.swpm_init.dram_ch_num = ch_num;
	offset = qos_ipi_to_sspm_command(&qos_d, 4);

	if (!offset) {
		share_idx_ref = NULL;
		share_idx_ctrl = NULL;
		idx_ref_uint_ptr = NULL;
		idx_output_size = 0;
		swpm_err("swpm share sram init fail\n");
		return;
	}

	/* get wrapped sram address */
	wrap_d = (struct share_wrap *)
		sspm_sbuf_get(offset);

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
}

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
	//get gpu lkg and thermal
	if (swpm_info_ref) {
		swpm_info_ref->gpu_reserved[gthermal + 1] =
			get_immediate_gpu_wrap() / 1000;
		swpm_info_ref->gpu_reserved[glkg + 1] =
			mt_gpufreq_get_leakage_no_lock();
	}
}

static int swpm_log_loop(void)
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

	return 0;
}

static inline void swpm_pass_to_sspm(void)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#ifdef CONFIG_MTK_DRAMC
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), get_emi_ch_num());
#else
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), 2);
#endif
#endif
}

static inline int swpm_init_pwr_data(void)
{
	swpm_lock(&swpm_mutex);

	/* copy pwr data */
	memcpy(swpm_info_ref->aphy_pwr_tbl, aphy_def_pwr_tbl,
		sizeof(aphy_def_pwr_tbl));
	memcpy(swpm_info_ref->dram_conf, dram_def_pwr_conf,
		sizeof(dram_def_pwr_conf));
	memcpy(swpm_info_ref->ddr_opp_freq, ddr_opp_freq,
		sizeof(ddr_opp_freq));

	swpm_unlock(&swpm_mutex);

	swpm_info("copy pwr data (size: aphy[%ld]/dram_conf[%ld]/dram_opp[%d])\n",
		(unsigned long)sizeof(aphy_def_pwr_tbl),
		(unsigned long)sizeof(dram_def_pwr_conf),
		(unsigned short)sizeof(ddr_opp_freq));

	return 0;
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

	mem_ref_tbl[GPU_POWER_METER].valid = true;
	mem_ref_tbl[GPU_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->gpu_reserved;
	mem_ref_tbl[ISP_POWER_METER].valid = true;
	mem_ref_tbl[ISP_POWER_METER].virt =
		(phys_addr_t *)&swpm_info_ref->isp_reserved;

	swpm_unlock(&swpm_mutex);
}

/***************************************************************************
 *  API
 ***************************************************************************/
void swpm_set_enable(unsigned int type, unsigned int enable)
{
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
	if (type != ALL_METER_TYPE && type >= NR_POWER_METER)
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

static int __init swpm_platform_init(void)
{
	int ret = 0;

#ifdef BRINGUP_DISABLE
	swpm_err("swpm is disabled\n");
	goto end;
#endif

	swpm_create_procfs();

	swpm_get_rec_addr(&rec_phys_addr,
			  &rec_virt_addr,
			  &rec_size);

	swpm_info_ref = (struct swpm_rec_data *)(uintptr_t)rec_virt_addr;

	if (!swpm_info_ref) {
		swpm_err("get sspm dram addr failed\n");
		ret = -1;
		goto end;
	}

	ret = swpm_reserve_mem_init(&rec_virt_addr, &rec_size);

	swpm_subsys_data_ref_init();

	swpm_init_pwr_data();

	ret = swpm_interface_manager_init(mem_ref_tbl, NR_SWPM_TYPE);

#if SWPM_TEST
	swpm_interface_unit_test();
#endif

	swpm_pass_to_sspm();

	/* set preiodic timer task */
	swpm_set_periodic_timer((void *)&swpm_log_loop);

#if SWPM_TEST
	/* enable all pwr meter and set swpm timer to start */
	swpm_set_enable(ALL_METER_TYPE, EN_POWER_METER_ONLY);
	swpm_update_periodic_timer();
#endif
end:
	return ret;
}

static void __exit swpm_platform_exit(void)
{
	swpm_set_enable(ALL_METER_TYPE, 0);
}
late_initcall_sync(swpm_platform_init);
module_exit(swpm_platform_exit);
MODULE_AUTHOR("www.mediatek.com>");
MODULE_DESCRIPTION("Software Power Meter");
