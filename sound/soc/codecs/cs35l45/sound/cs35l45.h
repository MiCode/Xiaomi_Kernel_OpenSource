/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * linux/sound/cs35l45.h -- Platform data for CS35L45
 *
 * Copyright 2019 Cirrus Logic, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */

#ifndef __CS35L45_H
#define __CS35L45_H

#define CS35L45_NUM_SUPPLIES 2

struct bst_bpe_inst_lvl_config {
	unsigned int thld;
	unsigned int ilim;
	unsigned int ss_ilim;
	unsigned int atk_rate;
	unsigned int hold_time;
	unsigned int rls_rate;
};

struct bst_bpe_inst_config {
	bool is_present;
	struct bst_bpe_inst_lvl_config l0;
	struct bst_bpe_inst_lvl_config l1;
	struct bst_bpe_inst_lvl_config l2;
	struct bst_bpe_inst_lvl_config l3;
	struct bst_bpe_inst_lvl_config l4;
};

struct bst_bpe_misc_config {
	bool is_present;
	unsigned int bst_bpe_inst_inf_hold_rls;
	unsigned int bst_bpe_il_lim_mode;
	unsigned int bst_bpe_out_opmode_sel;
	unsigned int bst_bpe_inst_l3_byp;
	unsigned int bst_bpe_inst_l2_byp;
	unsigned int bst_bpe_inst_l1_byp;
	unsigned int bst_bpe_filt_sel;
};

struct bst_bpe_il_lim_config {
	bool is_present;
	unsigned int bst_bpe_il_lim_thld_del1;
	unsigned int bst_bpe_il_lim_thld_del2;
	unsigned int bst_bpe_il_lim1_thld;
	unsigned int bst_bpe_il_lim1_dly;
	unsigned int bst_bpe_il_lim2_dly;
	unsigned int bst_bpe_il_lim_dly_hyst;
	unsigned int bst_bpe_il_lim_thld_hyst;
	unsigned int bst_bpe_il_lim1_atk_rate;
	unsigned int bst_bpe_il_lim2_atk_rate;
	unsigned int bst_bpe_il_lim1_rls_rate;
	unsigned int bst_bpe_il_lim2_rls_rate;
};

struct hvlv_config {
	bool is_present;
	unsigned int hvlv_thld_hys;
	unsigned int hvlv_thld;
	unsigned int hvlv_dly;
};

struct ldpm_config {
	bool is_present;
	unsigned int ldpm_gp1_boost_sel;
	unsigned int ldpm_gp1_amp_sel;
	unsigned int ldpm_gp1_delay;
	unsigned int ldpm_gp1_pcm_thld;
	unsigned int ldpm_gp2_imon_sel;
	unsigned int ldpm_gp2_vmon_sel;
	unsigned int ldpm_gp2_delay;
	unsigned int ldpm_gp2_pcm_thld;
};

struct classh_config {
	bool is_present;
	unsigned int ch_hdrm;
	unsigned int ch_ratio;
	unsigned int ch_rel_rate;
	unsigned int ch_ovb_thld1;
	unsigned int ch_ovb_thlddelta;
	unsigned int ch_vdd_bst_max;
	unsigned int ch_ovb_ratio;
	unsigned int ch_thld1_offset;
	unsigned int aud_mem_depth;
};

struct gpio_ctrl {
	bool is_present;
	unsigned int dir;
	unsigned int lvl;
	unsigned int op_cfg;
	unsigned int pol;
	unsigned int ctrl;
	unsigned int invert;
};

struct cs35l45_irq_bit_monitor {
	unsigned int bitmask;
	const char *description;
	const char *info_msg;
	const char *dbg_msg;
	const char *warn_msg;
	const char *err_msg;
	int (*callback)(struct cs35l45_private *cs35l45);
};

struct cs35l45_irq_monitor {
	unsigned int reg;
	unsigned int mask;
	unsigned int nbits;
	struct cs35l45_irq_bit_monitor *bits;
};

struct cs35l45_platform_data {
	struct bst_bpe_inst_config bst_bpe_inst_cfg;
	struct bst_bpe_misc_config bst_bpe_misc_cfg;
	struct bst_bpe_il_lim_config bst_bpe_il_lim_cfg;
	struct hvlv_config hvlv_cfg;
	struct ldpm_config ldpm_cfg;
	struct classh_config classh_cfg;
	struct gpio_ctrl gpio_ctrl1;
	struct gpio_ctrl gpio_ctrl2;
	struct gpio_ctrl gpio_ctrl3;
	const char *dsp_part_name;
	unsigned int asp_sdout_hiz_ctrl;
	unsigned int ngate_ch1_hold;
	unsigned int ngate_ch1_thr;
	unsigned int ngate_ch2_hold;
	unsigned int ngate_ch2_thr;
	unsigned int global_en_gpio;
	bool use_tdm_slots;
	bool allow_hibernate;
};

struct cs35l45_compr {
	struct wm_adsp *dsp;
	struct snd_compr_stream *stream;
	struct snd_compressed_buffer size;
	struct work_struct start_work;
	struct work_struct stop_work;
	u32 *raw_buf;
	unsigned int copied_total;
	unsigned int sample_rate;
	int read_index;
	int last_read_index;
	int buffer_size;
	int avail;
	int buffer_count;
};

struct cs35l45_private {
	struct wm_adsp dsp; /* needs to be first member */
	struct device *dev;
	struct regmap *regmap;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[CS35L45_NUM_SUPPLIES];
	struct cs35l45_platform_data pdata;
	struct cs35l45_compr *compr;
	struct work_struct dsp_pmu_work;
	struct work_struct dsp_pmd_work;
	struct delayed_work hb_work;
	struct delayed_work global_err_rls_work;
	struct workqueue_struct *wq;
	struct mutex dsp_power_lock;
	struct mutex hb_lock;
	struct completion virt2_mbox_comp;
	enum control_bus_type bus_type;
	bool initialized;
	bool fast_switch_en;
	bool hibernate_state;
	unsigned int i2c_addr;
	unsigned int speaker_status;
	int irq;
	int slot_width;
	int amplifier_mode;
	int hibernate_mode;
	int max_quirks_read_nwords;
	/* Run-time mixer */
	struct snd_kcontrol_new fast_ctl;
	unsigned int fast_switch_file_idx;
	struct soc_enum fast_switch_enum;
	const char **fast_switch_names;
	struct regmap_irq_chip_data *irq_data;
	struct snd_soc_component *component;
};

int cs35l45_initialize(struct cs35l45_private *cs35l45);
int cs35l45_probe(struct cs35l45_private *cs35l45);
int cs35l45_remove(struct cs35l45_private *cs35l45);

struct of_entry {
	const char *name;
	unsigned int reg;
	unsigned int mask;
	unsigned int shift;
};

enum bst_bpe_inst_level {
	L0 = 0,
	L1,
	L2,
	L3,
	L4,
	BST_BPE_INST_LEVELS
};

enum bst_bpe_inst_of_param {
	BST_BPE_INST_THLD = 0,
	BST_BPE_INST_ILIM,
	BST_BPE_INST_SS_ILIM,
	BST_BPE_INST_ATK_RATE,
	BST_BPE_INST_HOLD_TIME,
	BST_BPE_INST_RLS_RATE,
	BST_BPE_INST_PARAMS
};

enum bst_bpe_misc_of_param {
	BST_BPE_INST_INF_HOLD_RLS = 0,
	BST_BPE_IL_LIM_MODE,
	BST_BPE_OUT_OPMODE_SEL,
	BST_BPE_INST_L3_BYP,
	BST_BPE_INST_L2_BYP,
	BST_BPE_INST_L1_BYP,
	BST_BPE_FILT_SEL,
	BST_BPE_MISC_PARAMS
};

enum bst_bpe_il_lim_of_param {
	BST_BPE_IL_LIM_THLD_DEL1 = 0,
	BST_BPE_IL_LIM_THLD_DEL2,
	BST_BPE_IL_LIM1_THLD,
	BST_BPE_IL_LIM1_DLY,
	BST_BPE_IL_LIM2_DLY,
	BST_BPE_IL_LIM_DLY_HYST,
	BST_BPE_IL_LIM_THLD_HYST,
	BST_BPE_IL_LIM1_ATK_RATE,
	BST_BPE_IL_LIM2_ATK_RATE,
	BST_BPE_IL_LIM1_RLS_RATE,
	BST_BPE_IL_LIM2_RLS_RATE,
	BST_BPE_IL_LIM_PARAMS
};

enum ldpm_of_param {
	LDPM_GP1_BOOST_SEL = 0,
	LDPM_GP1_AMP_SEL,
	LDPM_GP1_DELAY,
	LDPM_GP1_PCM_THLD,
	LDPM_GP2_IMON_SEL,
	LDPM_GP2_VMON_SEL,
	LDPM_GP2_DELAY,
	LDPM_GP2_PCM_THLD,
	LDPM_PARAMS
};

enum classh_of_param {
	CH_HDRM = 0,
	CH_RATIO,
	CH_REL_RATE,
	CH_OVB_THLD1,
	CH_OVB_THLDDELTA,
	CH_VDD_BST_MAX,
	CH_OVB_RATIO,
	CH_THLD1_OFFSET,
	AUD_MEM_DEPTH,
	CLASSH_PARAMS
};

extern const struct of_entry bst_bpe_inst_thld_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_inst_ilim_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_inst_ss_ilim_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_inst_atk_rate_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_inst_hold_time_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_inst_rls_rate_map[BST_BPE_INST_LEVELS];
extern const struct of_entry bst_bpe_misc_map[BST_BPE_MISC_PARAMS];
extern const struct of_entry bst_bpe_il_lim_map[BST_BPE_IL_LIM_PARAMS];
extern const struct of_entry ldpm_map[LDPM_PARAMS];
extern const struct of_entry classh_map[CLASSH_PARAMS];

static inline const struct of_entry *cs35l45_get_bst_bpe_inst_entry(
					enum bst_bpe_inst_level level,
					enum bst_bpe_inst_of_param param)
{
	if ((level < L0) || (level > L4))
		return NULL;

	switch (param) {
	case BST_BPE_INST_THLD:
		return &bst_bpe_inst_thld_map[level];
	case BST_BPE_INST_ILIM:
		return &bst_bpe_inst_ilim_map[level];
	case BST_BPE_INST_SS_ILIM:
		return &bst_bpe_inst_ss_ilim_map[level];
	case BST_BPE_INST_ATK_RATE:
		return &bst_bpe_inst_atk_rate_map[level];
	case BST_BPE_INST_HOLD_TIME:
		return &bst_bpe_inst_hold_time_map[level];
	case BST_BPE_INST_RLS_RATE:
		return &bst_bpe_inst_rls_rate_map[level];
	default:
		return NULL;
	}
}

static inline u32 *cs35l45_get_bst_bpe_inst_param(
					struct cs35l45_private *cs35l45,
					enum bst_bpe_inst_level level,
					enum bst_bpe_inst_of_param param)
{
	struct bst_bpe_inst_lvl_config *cfg;

	switch (level) {
	case L0:
		cfg = &cs35l45->pdata.bst_bpe_inst_cfg.l0;
		break;
	case L1:
		cfg = &cs35l45->pdata.bst_bpe_inst_cfg.l1;
		break;
	case L2:
		cfg = &cs35l45->pdata.bst_bpe_inst_cfg.l2;
		break;
	case L3:
		cfg = &cs35l45->pdata.bst_bpe_inst_cfg.l3;
		break;
	case L4:
		cfg = &cs35l45->pdata.bst_bpe_inst_cfg.l4;
		break;
	default:
		return NULL;
	}

	switch (param) {
	case BST_BPE_INST_THLD:
		return &cfg->thld;
	case BST_BPE_INST_ILIM:
		return &cfg->ilim;
	case BST_BPE_INST_SS_ILIM:
		return &cfg->ss_ilim;
	case BST_BPE_INST_ATK_RATE:
		return &cfg->atk_rate;
	case BST_BPE_INST_HOLD_TIME:
		return &cfg->hold_time;
	case BST_BPE_INST_RLS_RATE:
		return &cfg->rls_rate;
	default:
		return NULL;
	}
}

static inline u32 *cs35l45_get_bst_bpe_misc_param(
					struct cs35l45_private *cs35l45,
					enum bst_bpe_misc_of_param param)
{
	struct bst_bpe_misc_config *cfg = &cs35l45->pdata.bst_bpe_misc_cfg;

	switch (param) {
	case BST_BPE_INST_INF_HOLD_RLS:
		return &cfg->bst_bpe_inst_inf_hold_rls;
	case BST_BPE_IL_LIM_MODE:
		return &cfg->bst_bpe_il_lim_mode;
	case BST_BPE_OUT_OPMODE_SEL:
		return &cfg->bst_bpe_out_opmode_sel;
	case BST_BPE_INST_L3_BYP:
		return &cfg->bst_bpe_inst_l3_byp;
	case BST_BPE_INST_L2_BYP:
		return &cfg->bst_bpe_inst_l2_byp;
	case BST_BPE_INST_L1_BYP:
		return &cfg->bst_bpe_inst_l1_byp;
	case BST_BPE_FILT_SEL:
		return &cfg->bst_bpe_filt_sel;
	default:
		return NULL;
	}
}

static inline u32 *cs35l45_get_bst_bpe_il_lim_param(
					struct cs35l45_private *cs35l45,
					enum bst_bpe_il_lim_of_param param)
{
	struct bst_bpe_il_lim_config *cfg = &cs35l45->pdata.bst_bpe_il_lim_cfg;

	switch (param) {
	case BST_BPE_IL_LIM_THLD_DEL1:
		return &cfg->bst_bpe_il_lim_thld_del1;
	case BST_BPE_IL_LIM_THLD_DEL2:
		return &cfg->bst_bpe_il_lim_thld_del2;
	case BST_BPE_IL_LIM1_THLD:
		return &cfg->bst_bpe_il_lim1_thld;
	case BST_BPE_IL_LIM1_DLY:
		return &cfg->bst_bpe_il_lim1_dly;
	case BST_BPE_IL_LIM2_DLY:
		return &cfg->bst_bpe_il_lim2_dly;
	case BST_BPE_IL_LIM_DLY_HYST:
		return &cfg->bst_bpe_il_lim_dly_hyst;
	case BST_BPE_IL_LIM_THLD_HYST:
		return &cfg->bst_bpe_il_lim_thld_hyst;
	case BST_BPE_IL_LIM1_ATK_RATE:
		return &cfg->bst_bpe_il_lim1_atk_rate;
	case BST_BPE_IL_LIM2_ATK_RATE:
		return &cfg->bst_bpe_il_lim2_atk_rate;
	case BST_BPE_IL_LIM1_RLS_RATE:
		return &cfg->bst_bpe_il_lim1_rls_rate;
	case BST_BPE_IL_LIM2_RLS_RATE:
		return &cfg->bst_bpe_il_lim2_rls_rate;
	default:
		return NULL;
	}
}

static inline u32 *cs35l45_get_ldpm_param(struct cs35l45_private *cs35l45,
					  enum ldpm_of_param param)
{
	struct ldpm_config *cfg = &cs35l45->pdata.ldpm_cfg;

	switch (param) {
	case LDPM_GP1_BOOST_SEL:
		return &cfg->ldpm_gp1_boost_sel;
	case LDPM_GP1_AMP_SEL:
		return &cfg->ldpm_gp1_amp_sel;
	case LDPM_GP1_DELAY:
		return &cfg->ldpm_gp1_delay;
	case LDPM_GP1_PCM_THLD:
		return &cfg->ldpm_gp1_pcm_thld;
	case LDPM_GP2_IMON_SEL:
		return &cfg->ldpm_gp2_imon_sel;
	case LDPM_GP2_VMON_SEL:
		return &cfg->ldpm_gp2_vmon_sel;
	case LDPM_GP2_DELAY:
		return &cfg->ldpm_gp2_delay;
	case LDPM_GP2_PCM_THLD:
		return &cfg->ldpm_gp2_pcm_thld;
	default:
		return NULL;
	}
}

static inline u32 *cs35l45_get_classh_param(struct cs35l45_private *cs35l45,
					    enum classh_of_param param)
{
	struct classh_config *cfg = &cs35l45->pdata.classh_cfg;

	switch (param) {
	case CH_HDRM:
		return &cfg->ch_hdrm;
	case CH_RATIO:
		return &cfg->ch_ratio;
	case CH_REL_RATE:
		return &cfg->ch_rel_rate;
	case CH_OVB_THLD1:
		return &cfg->ch_ovb_thld1;
	case CH_OVB_THLDDELTA:
		return &cfg->ch_ovb_thlddelta;
	case CH_VDD_BST_MAX:
		return &cfg->ch_vdd_bst_max;
	case CH_OVB_RATIO:
		return &cfg->ch_ovb_ratio;
	case CH_THLD1_OFFSET:
		return &cfg->ch_thld1_offset;
	case AUD_MEM_DEPTH:
		return &cfg->aud_mem_depth;
	default:
		return NULL;
	}
}

#endif /* __CS35L45_H */
