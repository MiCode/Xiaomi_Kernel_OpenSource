/* SPDX-License-Identifier: GPL-2.0
 *
 * energy_model.h - energy model header file
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */

#define MAX_NR_FREQ      32 /* Max supported number of frequency */
#define VOLT_STEP    625

#define EEM_LOG_BASE		0x00112400
#define EEM_LOG_SIZE		0x1800 /* 6k size 0x1800 */

#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define OFFS_CAP_TBL      0x0FA0

#define DVFS_TBL_BASE_PHYS 0x0011BC00
#define CAPACITY_TBL_OFFSET 0xFA0
#define CAPACITY_TBL_SIZE 0x100
#define CAPACITY_ENTRY_SIZE 0x2

#define MAX_PD_COUNT 3

#define MAX_LEAKAGE_SIZE	36
#define FREQ_STEP			26000

extern struct mtk_em_perf_domain *mtk_em_pd_ptr_public;
extern struct mtk_em_perf_domain *mtk_em_pd_ptr_private;

struct para {
	unsigned int a: 12;
	unsigned int b: 20;
};

struct lkg_para {
	struct para a_b_para;
	unsigned int c;
};

struct mtk_em_perf_state {
	/* Performance state setting */
	unsigned int freq;
	unsigned int volt;
	unsigned int capacity;
	unsigned int dyn_pwr;
	struct lkg_para leakage_para;
	unsigned int pwr_eff;
};

struct leakage_para {
	int a_b_para[36];
	int c[36];
};

struct leakage_data {
	void __iomem *base;
	int init;
};

struct mtk_em_perf_domain {
	struct mtk_em_perf_state *table;
	unsigned int nr_perf_states;
	unsigned int cluster_num;
	unsigned int max_freq;
	unsigned int min_freq;
	struct cpumask *cpumask;
};

enum eemsn_det_id {
	EEMSN_DET_L = 0,
	EEMSN_DET_BL, /* for BL or B */
	EEMSN_DET_B, /* for B or DSU */
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

struct eemsn_log_det {
	unsigned int temp;
	unsigned short freq_tbl[MAX_NR_FREQ];
	unsigned char volt_tbl_pmic[MAX_NR_FREQ];
	unsigned char volt_tbl_orig[MAX_NR_FREQ];
	unsigned char volt_tbl_init2[MAX_NR_FREQ];
	unsigned char num_freq_tbl;
	unsigned char lock;
	unsigned char features;
	int8_t volt_clamp;
	int8_t volt_offset;
	enum eemsn_det_id det_id;
};

struct eemsn_log {
	unsigned int eemsn_disable:8;
	unsigned int ctrl_aging_Enable:8;
	unsigned int sn_disable:8;
	unsigned int segCode:8;
	unsigned char init2_v_ready;
	unsigned char init_vboot_done;
	unsigned char lock;
	unsigned char eemsn_log_en;
	struct eemsn_log_det det_log[NR_EEMSN_DET];
};

static inline unsigned int mtk_get_nr_cap(unsigned int cluster)
{
	return mtk_em_pd_ptr_private[cluster].nr_perf_states;
}

unsigned int mtk_get_leakage(unsigned int cluster, unsigned int opp, unsigned int temperature);
extern __init int mtk_static_power_init(void);
