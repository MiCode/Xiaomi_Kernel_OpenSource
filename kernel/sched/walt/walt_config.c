// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "walt.h"
#include "trace.h"
#include <soc/qcom/socinfo.h>

unsigned long __read_mostly soc_flags;
unsigned int trailblazer_floor_freq[MAX_CLUSTERS];
cpumask_t asym_cap_sibling_cpus;
//MIUI_ADD: Task_Attribute_Sched
unsigned int __read_mostly miui_power_enhance;
//END Task_Attribute_Sched
cpumask_t pipeline_sync_cpus;
cpumask_t storage_boost_cpus;
int oscillate_period_ns;
int soc_sched_lib_name_capacity;
#define PIPELINE_BUSY_THRESH_8MS_WINDOW 7
#define PIPELINE_BUSY_THRESH_12MS_WINDOW 11
#define PIPELINE_BUSY_THRESH_16MS_WINDOW 15
unsigned int soc_cluster_freq_table_size[MAX_CLUSTERS];
unsigned int soc_cluster_freq_table[MAX_CLUSTERS][MAX_FREQ_TABLE_ENTRIES];
unsigned int demand_scaling_factor;
unsigned int min_demand_for_activity_cnt;
void walt_config(void)
{
	int i, j, cpu;
	const char *name = socinfo_get_id_string();

	sysctl_sched_group_upmigrate_pct = 100;
	sysctl_sched_group_downmigrate_pct = 95;
	sysctl_sched_task_unfilter_period = 100000000;
	sysctl_sched_window_stats_policy = WINDOW_STATS_MAX_RECENT_AVG;
	sysctl_sched_ravg_window_nr_ticks = (HZ / NR_WINDOWS_PER_SEC);
	sched_load_granule = DEFAULT_SCHED_RAVG_WINDOW / NUM_LOAD_INDICES;
	sysctl_sched_coloc_busy_hyst_enable_cpus = 112;
	sysctl_sched_util_busy_hyst_enable_cpus = 255;
	sysctl_sched_coloc_busy_hyst_max_ms = 5000;
	sched_ravg_window = DEFAULT_SCHED_RAVG_WINDOW;
// MIUI DEL: Performance_BoostFramework
//	sysctl_input_boost_ms = 40;
// END Performance_BoostFramework
//MIUI_ADD: Task_Attribute_Sched
        miui_power_enhance = 0;
//END Task_Attribute_Sched
	sysctl_sched_min_task_util_for_boost = 51;
	sysctl_sched_min_task_util_for_uclamp = 51;
	sysctl_sched_min_task_util_for_colocation = 35;
	sysctl_sched_many_wakeup_threshold = WALT_MANY_WAKEUP_DEFAULT;
	sysctl_walt_rtg_cfs_boost_prio = 99; /* disabled by default */
	sysctl_sched_sync_hint_enable = 1;
	sysctl_panic_on_walt_bug = walt_debug_initial_values();
	sysctl_sched_skip_sp_newly_idle_lb = 1;
	sysctl_sched_hyst_min_coloc_ns = 80000000;
	sysctl_sched_idle_enough = SCHED_IDLE_ENOUGH_DEFAULT;
	sysctl_sched_cluster_util_thres_pct = SCHED_CLUSTER_UTIL_THRES_PCT_DEFAULT;
	sysctl_em_inflate_pct = 100;
	sysctl_em_inflate_thres = 1024;
	sysctl_max_freq_partial_halt = FREQ_QOS_MAX_DEFAULT_VALUE;
	sysctl_topapp_weight_pct = 100;
	asym_cap_sibling_cpus = CPU_MASK_NONE;
	storage_boost_cpus = CPU_MASK_NONE;

	/* pipeline defaults */
	pipeline_sync_cpus = CPU_MASK_NONE;
	demand_scaling_factor = 100;
	min_demand_for_activity_cnt = 50;

	for_each_possible_cpu(cpu) {
		for (i = 0; i < LEGACY_SMART_FREQ; i++) {
			if (i)
				smart_freq_legacy_reason_hyst_ms[i][cpu] = 4;
			else
				smart_freq_legacy_reason_hyst_ms[i][cpu] = 0;
		}
	}

	for (i = 0; i < MAX_MARGIN_LEVELS; i++) {
		sysctl_sched_capacity_margin_up_pct[i] = 95; /* ~5% margin */
		sysctl_sched_capacity_margin_dn_pct[i] = 85; /* ~15% margin */
		sysctl_sched_early_up[i] = 1077;
		sysctl_sched_early_down[i] = 1204;
	}

	for (i = 0; i < WALT_NR_CPUS; i++) {
		sysctl_sched_coloc_busy_hyst_cpu[i] = 39000000;
		sysctl_sched_coloc_busy_hyst_cpu_busy_pct[i] = 10;
		sysctl_sched_util_busy_hyst_cpu[i] = 5000000;
		sysctl_sched_util_busy_hyst_cpu_util[i] = 15;
// MIUI DEL: Performance_BoostFramework
//		sysctl_input_boost_freq[i] = 0;
// END Performance_BoostFramework
	}

	for (i = 0; i < MAX_CLUSTERS; i++) {
		sysctl_freq_cap[i] = FREQ_QOS_MAX_DEFAULT_VALUE;
		high_perf_cluster_freq_cap[i] = FREQ_QOS_MAX_DEFAULT_VALUE;
		sysctl_sched_idle_enough_clust[i] = SCHED_IDLE_ENOUGH_DEFAULT;
		sysctl_sched_cluster_util_thres_pct_clust[i] = SCHED_CLUSTER_UTIL_THRES_PCT_DEFAULT;
		trailblazer_floor_freq[i] = 0;
		for (j = 0; j < MAX_CLUSTERS; j++) {
			load_sync_util_thres[i][j] = 0;
			load_sync_low_pct[i][j] = 0;
			load_sync_high_pct[i][j] = 0;
		}
	}

	for (i = 0; i < MAX_FREQ_CAP; i++) {
		for (j = 0; j < MAX_CLUSTERS; j++)
			freq_cap[i][j] = FREQ_QOS_MAX_DEFAULT_VALUE;
	}

	sysctl_sched_lrpb_active_ms[0] = PIPELINE_BUSY_THRESH_8MS_WINDOW;
	sysctl_sched_lrpb_active_ms[1] = PIPELINE_BUSY_THRESH_12MS_WINDOW;
	sysctl_sched_lrpb_active_ms[2] = PIPELINE_BUSY_THRESH_16MS_WINDOW;
	soc_feat_set(SOC_ENABLE_CONSERVATIVE_BOOST_TOPAPP_BIT);
	soc_feat_set(SOC_ENABLE_CONSERVATIVE_BOOST_FG_BIT);
	soc_feat_set(SOC_ENABLE_UCLAMP_BOOSTED_BIT);
	soc_feat_set(SOC_ENABLE_PER_TASK_BOOST_ON_MID_BIT);
	soc_feat_set(SOC_ENABLE_COLOCATION_PLACEMENT_BOOST_BIT);
	soc_feat_set(SOC_ENABLE_PIPELINE_SWAPPING_BIT);
	soc_feat_set(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);

	pipeline_swap_util_th = 0;

	/* Initialize smart freq configurations */
	smart_freq_init(name);
	/* return if socinfo is not available */
	if (!name)
		return;

	if (!strcmp(name, "SUN") || !strcmp(name, "SUNP") || !strcmp(name, "CANOE")
			|| !strcmp(name, "ALOR_INTERPOSER") || !strcmp(name, "ALOR")
			|| !strcmp(name, "ALORP")
			|| !strcmp(name, "WHALE") || !strcmp(name, "WHALEP")
			|| !strcmp(name, "CANOEPSG") || !strcmp(name, "CANOEP")) {
		sysctl_sched_suppress_region2		= 1;
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_TOPAPP_BIT);
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_FG_BIT);
		soc_feat_unset(SOC_ENABLE_UCLAMP_BOOSTED_BIT);
		soc_feat_unset(SOC_ENABLE_PER_TASK_BOOST_ON_MID_BIT);
		trailblazer_floor_freq[0] = 2500000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;
		sysctl_walt_features |= WALT_FEAT_TOPAPP_BASED_HISPEED;
		soc_feat_unset(SOC_ENABLE_COLOCATION_PLACEMENT_BOOST_BIT);
		soc_feat_set(SOC_ENABLE_FT_BOOST_TO_ALL);
		oscillate_period_ns = 8000000;
		/*G + P*/
		cpumask_copy(&pipeline_sync_cpus, cpu_possible_mask);
		cpumask_copy(&storage_boost_cpus, cpu_possible_mask);
		soc_sched_lib_name_capacity = 2;
		soc_feat_unset(SOC_ENABLE_PIPELINE_SWAPPING_BIT);
		pipeline_swap_util_th = 50;

		sysctl_cluster01_load_sync[0]	= 350;
		sysctl_cluster01_load_sync[1]	= 100;
		sysctl_cluster01_load_sync[2]	= 100;
		sysctl_cluster10_load_sync[0]	= 512;
		sysctl_cluster10_load_sync[1]	= 90;
		sysctl_cluster10_load_sync[2]	= 90;
		load_sync_util_thres[0][1]	= sysctl_cluster01_load_sync[0];
		load_sync_low_pct[0][1]		= sysctl_cluster01_load_sync[1];
		load_sync_high_pct[0][1]	= sysctl_cluster01_load_sync[2];
		load_sync_util_thres[1][0]	= sysctl_cluster10_load_sync[0];
		load_sync_low_pct[1][0]		= sysctl_cluster10_load_sync[1];
		load_sync_high_pct[1][0]	= sysctl_cluster10_load_sync[2];

		sysctl_cluster01_load_sync_60fps[0]	= 400;
		sysctl_cluster01_load_sync_60fps[1]	= 60;
		sysctl_cluster01_load_sync_60fps[2]	= 100;
		sysctl_cluster10_load_sync_60fps[0]	= 500;
		sysctl_cluster10_load_sync_60fps[1]	= 70;
		sysctl_cluster10_load_sync_60fps[2]	= 90;
		load_sync_util_thres_60fps[0][1]	= sysctl_cluster01_load_sync_60fps[0];
		load_sync_low_pct_60fps[0][1]		= sysctl_cluster01_load_sync_60fps[1];
		load_sync_high_pct_60fps[0][1]		= sysctl_cluster01_load_sync_60fps[2];
		load_sync_util_thres_60fps[1][0]	= sysctl_cluster10_load_sync_60fps[0];
		load_sync_low_pct_60fps[1][0]		= sysctl_cluster10_load_sync_60fps[1];
		load_sync_high_pct_60fps[1][0]		= sysctl_cluster10_load_sync_60fps[2];

		/* CPU0 needs an 9mS bias for all legacy smart freq reasons */
		for (i = 1; i < LEGACY_SMART_FREQ; i++)
			smart_freq_legacy_reason_hyst_ms[i][0] = 9;
		for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
			for (i = 1; i < LEGACY_SMART_FREQ; i++)
				smart_freq_legacy_reason_hyst_ms[i][cpu] = 2;
		}
		for_each_possible_cpu(cpu) {
			smart_freq_legacy_reason_hyst_ms[PIPELINE_60FPS_OR_LESSER_SMART_FREQ][cpu] =
				1;
		}
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);
		if (strcmp(name, "ALOR_INTERPOSER") && strcmp(name, "ALOR")
				&& strcmp(name, "ALORP"))
			demand_scaling_factor = 70;
		/*
		 * By default this SOC flag will be disabled. Enable this only
		 * for Alor platforms
		 */
		if (!strcmp(name, "ALOR_INTERPOSER") || !strcmp(name, "ALOR")
				|| !strcmp(name, "ALORP"))
			soc_feat_set(SOC_ENABLE_LIMIT_PRIME_USAGE);

	} else if (!strcmp(name, "PINEAPPLE")) {
		soc_feat_set(SOC_ENABLE_SILVER_RT_SPREAD_BIT);
		soc_feat_set(SOC_ENABLE_BOOST_TO_NEXT_CLUSTER_BIT);

		/* T + G */
		cpumask_or(&asym_cap_sibling_cpus,
			&asym_cap_sibling_cpus, &cpu_array[0][1]);
		cpumask_or(&asym_cap_sibling_cpus,
			&asym_cap_sibling_cpus, &cpu_array[0][2]);

		/* T + G + P */
		cpumask_andnot(&storage_boost_cpus, cpu_possible_mask, &cpu_array[0][0]);

		/*
		 * Treat Golds and Primes as candidates for load sync under pipeline usecase.
		 * However, it is possible that a single CPU is not present. As prime is the
		 * only cluster with only one CPU, guard this setting by ensuring 4 clusters
		 * are present.
		 */
		if (num_sched_clusters == 4) {
			cpumask_or(&pipeline_sync_cpus,
				&pipeline_sync_cpus, &cpu_array[0][2]);
			cpumask_or(&pipeline_sync_cpus,
				&pipeline_sync_cpus, &cpu_array[0][3]);
		}

	} else if (!strcmp(name, "TUNA")) {
		soc_feat_set(SOC_ENABLE_SILVER_RT_SPREAD_BIT);
		soc_feat_set(SOC_ENABLE_BOOST_TO_NEXT_CLUSTER_BIT);
		soc_feat_set(SOC_ENABLE_SINGLE_THREAD_PIPELINE_PINNING);
		soc_sched_lib_name_capacity = 2;
		/*
		 * Treat Golds and Primes as candidates for load sync under pipeline usecase.
		 * However, it is possible that a single CPU is not present. As prime is the
		 * only cluster with only one CPU, guard this setting by ensuring 4 clusters
		 * are present.
		 */
		if (num_sched_clusters == 4) {
			cpumask_or(&pipeline_sync_cpus,
				&pipeline_sync_cpus, &cpu_array[0][2]);
			cpumask_or(&pipeline_sync_cpus,
				&pipeline_sync_cpus, &cpu_array[0][3]);
		}
		pipeline_swap_util_th = 100;

		/*
		 * Trailblazer settings
		 */
		trailblazer_floor_freq[0] = 1000000;
		trailblazer_floor_freq[1] = 1000000;
		trailblazer_floor_freq[2] = 1000000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;

		/*
		 * Do not put the whole cluster at Fmin during thermal halt condition.
		 */
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);

		sysctl_sched_suppress_region2 = 1;
	} else if (!strcmp(name, "KERA")) {
		soc_sched_lib_name_capacity = 3;
		/*
		 * Trailblazer settings
		 */
		trailblazer_floor_freq[0] = 1000000;
		trailblazer_floor_freq[1] = 1000000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		pipeline_swap_util_th = 100;
		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;

		/*
		 * Do not put the whole cluster at Fmin during thermal halt condition.
		 */
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);

	} else if (!strcmp(name, "CHORA")) {
		soc_sched_lib_name_capacity = 4;
		/*
		 * Trailblazer settings
		 */
		trailblazer_floor_freq[0] = 1000000;
		trailblazer_floor_freq[1] = 1000000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		pipeline_swap_util_th = 100;
		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;

		/*
		 * Do not put the whole cluster at Fmin during thermal halt condition.
		 */
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);

	} else if (!strcmp(name, "MALABAR")) {
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_TOPAPP_BIT);
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_FG_BIT);
		soc_feat_unset(SOC_ENABLE_UCLAMP_BOOSTED_BIT);
		soc_feat_unset(SOC_ENABLE_PER_TASK_BOOST_ON_MID_BIT);

		// Evaluate and change the trailblazer freq as per need in future.
		trailblazer_floor_freq[0] = 1000000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;
		sysctl_walt_features |= WALT_FEAT_TOPAPP_BASED_HISPEED;
		soc_feat_unset(SOC_ENABLE_COLOCATION_PLACEMENT_BOOST_BIT);
		soc_feat_set(SOC_ENABLE_FT_BOOST_TO_ALL);
		cpumask_copy(&storage_boost_cpus, cpu_possible_mask);
		soc_feat_unset(SOC_ENABLE_PIPELINE_SWAPPING_BIT);

		soc_sched_lib_name_capacity = 2;
		// Evaluate and change the swap util thres as per need in future.
		pipeline_swap_util_th = 50;

		/* CPU0 needs an 9mS bias for all legacy smart freq reasons */
		for (i = 1; i < LEGACY_SMART_FREQ; i++)
			smart_freq_legacy_reason_hyst_ms[i][0] = 9;
		for_each_cpu(cpu, &cpu_array[0][num_sched_clusters - 1]) {
			for (i = 1; i < LEGACY_SMART_FREQ; i++)
				smart_freq_legacy_reason_hyst_ms[i][cpu] = 2;
		}
		for_each_possible_cpu(cpu) {
			smart_freq_legacy_reason_hyst_ms[PIPELINE_60FPS_OR_LESSER_SMART_FREQ][cpu] =
				1;
		}
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);

	} else if (!strcmp(name, "VIENNA") || !strcmp(name, "VIENNAP")) {
		/*
		 * Do not put the whole cluster at Fmin during thermal halt condition.
		 */
		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);
	} else if (!strcmp(name, "X1E80100")) {
		/*
		 * By default this SOC flag will be disabled. Enable this only
		 * for X1E80100 platforms
		 */
		trailblazer_floor_freq[0] = 2500000;
		trailblazer_floor_freq[1] = 2500000;
		sysctl_walt_features |= WALT_FEAT_TRAILBLAZER_BIT;
		soc_feat_set(SOC_ENABLE_SW_CYCLE_COUNTER_BIT);
	} else if (!strcmp(name, "SERAPH") || !strcmp(name, "PIKACHU")) {
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_TOPAPP_BIT);
		soc_feat_unset(SOC_ENABLE_CONSERVATIVE_BOOST_FG_BIT);
		soc_feat_unset(SOC_ENABLE_UCLAMP_BOOSTED_BIT);
		soc_feat_unset(SOC_ENABLE_PER_TASK_BOOST_ON_MID_BIT);

		sysctl_walt_features |= WALT_FEAT_SYNC_FREQ_CAP_BIT;
		sysctl_walt_features |= WALT_FEAT_TOPAPP_BASED_HISPEED;
		soc_feat_unset(SOC_ENABLE_COLOCATION_PLACEMENT_BOOST_BIT);
		soc_feat_set(SOC_ENABLE_FT_BOOST_TO_ALL);
		cpumask_copy(&storage_boost_cpus, cpu_possible_mask);
		soc_sched_lib_name_capacity = 4;


		soc_feat_unset(SOC_ENABLE_THERMAL_HALT_LOW_FREQ_BIT);
	}

}

void early_walt_config(void)
{
	const char *name = socinfo_get_id_string();

	if (!name)
		return;

	memset(soc_cluster_freq_table_size, 0, sizeof(soc_cluster_freq_table_size));
	memset(soc_cluster_freq_table, 0, sizeof(soc_cluster_freq_table));
	if (!strcmp(name, "SUN") || !strcmp(name, "SUNP")) {
		soc_cluster_freq_table_size[0] = 16;
		soc_cluster_freq_table_size[1] = 16;

		soc_cluster_freq_table[0][0] = 683;
		soc_cluster_freq_table[0][1] = 731;
		soc_cluster_freq_table[0][2] = 782;
		soc_cluster_freq_table[0][3] = 792;
		soc_cluster_freq_table[0][4] = 813;
		soc_cluster_freq_table[0][5] = 856;
		soc_cluster_freq_table[0][6] = 884;
		soc_cluster_freq_table[0][7] = 958;
		soc_cluster_freq_table[0][8] = 1004;
		soc_cluster_freq_table[0][9] = 1087;
		soc_cluster_freq_table[0][10] = 1153;
		soc_cluster_freq_table[0][11] = 1300;
		soc_cluster_freq_table[0][12] = 1462;
		soc_cluster_freq_table[0][13] = 1629;
		soc_cluster_freq_table[0][14] = 1894;
		soc_cluster_freq_table[0][15] = 2183;

		soc_cluster_freq_table[1][0] = 1655;
		soc_cluster_freq_table[1][1] = 1749;
		soc_cluster_freq_table[1][2] = 1775;
		soc_cluster_freq_table[1][3] = 1951;
		soc_cluster_freq_table[1][4] = 2104;
		soc_cluster_freq_table[1][5] = 2268;
		soc_cluster_freq_table[1][6] = 2425;
		soc_cluster_freq_table[1][7] = 2530;
		soc_cluster_freq_table[1][8] = 2652;
		soc_cluster_freq_table[1][9] = 2903;
		soc_cluster_freq_table[1][10] = 3225;
		soc_cluster_freq_table[1][11] = 3592;
		soc_cluster_freq_table[1][12] = 4384;
		soc_cluster_freq_table[1][13] = 5087;
		soc_cluster_freq_table[1][14] = 5390;
		soc_cluster_freq_table[1][15] = 5516;
	} else if (!strcmp(name, "CANOE") || !strcmp(name, "CANOEPSG") || !strcmp(name, "WHALE")
			|| !strcmp(name, "WHALEP") || !strcmp(name, "CANOEP")) {
		soc_cluster_freq_table_size[0] = 32;
		soc_cluster_freq_table_size[1] = 32;

		soc_cluster_freq_table[0][0] = 548;
		soc_cluster_freq_table[0][1] = 720;
		soc_cluster_freq_table[0][2] = 816;
		soc_cluster_freq_table[0][3] = 857;
		soc_cluster_freq_table[0][4] = 889;
		soc_cluster_freq_table[0][5] = 919;
		soc_cluster_freq_table[0][6] = 943;
		soc_cluster_freq_table[0][7] = 971;
		soc_cluster_freq_table[0][8] = 991;
		soc_cluster_freq_table[0][9] = 1011;
		soc_cluster_freq_table[0][10] = 1029;
		soc_cluster_freq_table[0][11] = 1068;
		soc_cluster_freq_table[0][12] = 1102;
		soc_cluster_freq_table[0][13] = 1131;
		soc_cluster_freq_table[0][14] = 1169;
		soc_cluster_freq_table[0][15] = 1209;
		soc_cluster_freq_table[0][16] = 1245;
		soc_cluster_freq_table[0][17] = 1318;
		soc_cluster_freq_table[0][18] = 1397;
		soc_cluster_freq_table[0][19] = 1458;
		soc_cluster_freq_table[0][20] = 1524;
		soc_cluster_freq_table[0][21] = 1724;
		soc_cluster_freq_table[0][22] = 1882;
		soc_cluster_freq_table[0][23] = 2047;
		soc_cluster_freq_table[0][24] = 2149;
		soc_cluster_freq_table[0][25] = 2256;
		soc_cluster_freq_table[0][26] = 2369;
		soc_cluster_freq_table[0][27] = 2525;
		soc_cluster_freq_table[0][28] = 2776;
		soc_cluster_freq_table[0][29] = 2974;
		soc_cluster_freq_table[0][30] = 3199;
		soc_cluster_freq_table[0][31] = 3377;

		soc_cluster_freq_table[1][0] = 1889;
		soc_cluster_freq_table[1][1] = 1917;
		soc_cluster_freq_table[1][2] = 1943;
		soc_cluster_freq_table[1][3] = 1960;
		soc_cluster_freq_table[1][4] = 1976;
		soc_cluster_freq_table[1][5] = 1987;
		soc_cluster_freq_table[1][6] = 2068;
		soc_cluster_freq_table[1][7] = 2148;
		soc_cluster_freq_table[1][8] = 2206;
		soc_cluster_freq_table[1][9] = 2306;
		soc_cluster_freq_table[1][10] = 2394;
		soc_cluster_freq_table[1][11] = 2484;
		soc_cluster_freq_table[1][12] = 2553;
		soc_cluster_freq_table[1][13] = 2681;
		soc_cluster_freq_table[1][14] = 2815;
		soc_cluster_freq_table[1][15] = 2955;
		soc_cluster_freq_table[1][16] = 3158;
		soc_cluster_freq_table[1][17] = 3343;
		soc_cluster_freq_table[1][18] = 3486;
		soc_cluster_freq_table[1][19] = 3661;
		soc_cluster_freq_table[1][20] = 3844;
		soc_cluster_freq_table[1][21] = 4420;
		soc_cluster_freq_table[1][22] = 4900;
		soc_cluster_freq_table[1][23] = 5400;
		soc_cluster_freq_table[1][24] = 5800;
		soc_cluster_freq_table[1][25] = 6536;
		soc_cluster_freq_table[1][26] = 7217;
		soc_cluster_freq_table[1][27] = 7696;
		soc_cluster_freq_table[1][28] = 8244;
		soc_cluster_freq_table[1][29] = 8680;
		soc_cluster_freq_table[1][30] = 9099;
		soc_cluster_freq_table[1][31] = 9502;
	}

}
