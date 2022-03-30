/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRAMC_H__
#define __DRAMC_H__

struct mr_info_t {
	unsigned int mr_index;
	unsigned int mr_value;
};

struct reg_ctrl_t {
	unsigned int offset;
	unsigned int mask;
	unsigned int shift;
};

struct fmeter_dev_t {
	unsigned int version;
	unsigned int crystal_freq;
	unsigned int shu_of;
	struct reg_ctrl_t shu_lv;
	struct reg_ctrl_t pll_id;
	struct reg_ctrl_t pll_md[2];
	struct reg_ctrl_t sdmpcw[2];
	struct reg_ctrl_t prediv[2];
	struct reg_ctrl_t posdiv[2];
	struct reg_ctrl_t ckdiv4[2];
	struct reg_ctrl_t cldiv2[2];
	struct reg_ctrl_t fbksel[2];
	struct reg_ctrl_t dqopen[2];
	struct reg_ctrl_t dqsopen[2];
	struct reg_ctrl_t ckdiv4_ca[2];
};

struct mr4_dev_t {
	unsigned int version;
	struct reg_ctrl_t mr4_rg;
};

struct dramc_dev_t {
	unsigned int dram_type;
	unsigned int support_ch_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned int mr_cnt;
	unsigned int freq_cnt;
	unsigned int *rk_size;
	unsigned int *freq_step;
	struct mr_info_t *mr_info_ptr;
	void __iomem **dramc_chn_base_ao;
	void __iomem **dramc_chn_base_nao;
	void __iomem **ddrphy_chn_base_ao;
	void __iomem **ddrphy_chn_base_nao;
	void __iomem *sleep_base;
	void *mr4_dev_ptr;
	void *fmeter_dev_ptr;
};

enum DRAM_TYPE {
	TYPE_DDR1 = 1,
	TYPE_LPDDR2,
	TYPE_LPDDR3,
	TYPE_PCDDR3,
	TYPE_LPDDR4,
	TYPE_LPDDR4X,
	TYPE_LPDDR4P,
	TYPE_LPDDR5,
	TYPE_LPDDR5X
};

#define DPM_IRQ_CHA	0
#define DPM_IRQ_CHB	1

int mtk_dramc_get_steps_freq(unsigned int step);
unsigned int mtk_dramc_get_ddr_type(void);
unsigned int mtk_dramc_get_data_rate(void);
unsigned int mtk_dramc_get_mr4(unsigned int ch);

#endif /* __DRAMC_H__ */

