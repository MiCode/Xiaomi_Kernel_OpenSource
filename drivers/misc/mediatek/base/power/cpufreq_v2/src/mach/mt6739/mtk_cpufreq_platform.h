/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_CPUFREQ_PLATFORM_H__
#define __MTK_CPUFREQ_PLATFORM_H__

#include "mtk_cpufreq_internal.h"

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define CONFIG_HYBRID_CPU_DVFS	1
#ifndef CONFIG_MTK_ACAO_SUPPORT
#define ENABLE_TURBO_MODE_AP	1
#endif
#define PPM_AP_SIDE	1
#define EEM_AP_SIDE	1
/* #define CPU_DVFS_NOT_READY	1 */
#endif

#define NR_FREQ                16

/* buck ctrl configs */
#define PMIC_STEP	625
#define NORMAL_DIFF_VRSAM_VPROC		10000
#define MAX_DIFF_VSRAM_VPROC		20000
#define MIN_VSRAM_VOLT			115000
#define MAX_VSRAM_VOLT			131250
#define MIN_VPROC_VOLT			95000
#define MAX_VPROC_VOLT			130625

#define PMIC_CMD_DELAY_TIME	5
#define MIN_PMIC_SETTLE_TIME	5

#define PLL_SETTLE_TIME		20
#define POS_SETTLE_TIME		1

#define DVFSP_DT_NODE		"mediatek,mt6763-dvfsp"

#define CSRAM_BASE		0x0011bc00
#define CSRAM_SIZE		0x1400		/* 5K bytes */

#define DVFS_LOG_NUM		150
#define ENTRY_EACH_LOG		5

#define TOTAL_CORE_NUM  (CORE_NUM_L)
#define CORE_NUM_L      (4)

#define get_cluster_cpu_core(id) (id ? 0 : CORE_NUM_L)

static inline int arch_get_nr_clusters(void)
{
	return 1;
}

static inline void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	int cpu = 0;

	cpumask_clear(cpus);

	if (cluster_id == 0) {
		cpu = 0;

		while (cpu < CORE_NUM_L) {
			cpumask_set_cpu(cpu, cpus);
			cpu++;
		}
	} else {
		cpu = CORE_NUM_L;

		while (cpu < TOTAL_CORE_NUM) {
			cpumask_set_cpu(cpu, cpus);
			cpu++;
		}
	}
}

static inline int arch_get_cluster_id(unsigned int cpu)
{
	return cpu < 4 ? 0:1;
}
extern struct mt_cpu_dvfs cpu_dvfs[NR_MT_CPU_DVFS];
extern struct buck_ctrl_t buck_ctrl[NR_MT_BUCK];
extern struct pll_ctrl_t pll_ctrl[NR_MT_PLL];
extern struct hp_action_tbl cpu_dvfs_hp_action[];
extern unsigned int nr_hp_action;

extern void prepare_pll_addr(enum mt_cpu_dvfs_pll_id pll_id);
extern void prepare_pmic_config(struct mt_cpu_dvfs *p);
extern unsigned int _cpu_dds_calc(unsigned int khz);
extern unsigned int get_cur_phy_freq(struct pll_ctrl_t *pll_p);
extern unsigned char get_clkdiv(struct pll_ctrl_t *pll_p);
extern unsigned char get_posdiv(struct pll_ctrl_t *pll_p);

extern int mt_cpufreq_regulator_map(struct platform_device *pdev);
extern int mt_cpufreq_dts_map(void);
extern unsigned int _mt_cpufreq_get_cpu_level(void);
extern unsigned int _mt_cpufreq_disable_feature(void);

#endif	/* __MTK_CPUFREQ_PLATFORM_H__ */
