/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __MMDVFS_CONFIG_UTIL_H__
#define __MMDVFS_CONFIG_UTIL_H__

#include <linux/module.h>
#include <linux/clk.h>
#include <mt-plat/mtk_smi.h>
#include "mmdvfs_mgr.h"

#define MMDVFS_CLK_BY_MUX	(0)
#define MMDVFS_CLK_PLL_RATE	(1)
#define MMDVFS_CLK_NONE		(2)

/* System OPP number limitation (Max opp support)*/
#define MMDVFS_OPP_NUM_LIMITATION		(6)
#define MMDVFS_CLK_OPP_NUM_LIMITATION		(6)
#define MMDVFS_VCORE_OPP_NUM_LIMITATION		(6)

/* MMDVFS OPPs (External interface, Vcore and MMCLK combined)*/
#define MMDVFS_FINE_STEP_UNREQUEST (-1)
#define MMDVFS_FINE_STEP_OPP0 (0)
#define MMDVFS_FINE_STEP_OPP1 (1)
#define MMDVFS_FINE_STEP_OPP2 (2)
#define MMDVFS_FINE_STEP_OPP3 (3)
#define MMDVFS_FINE_STEP_OPP4 (4)
#define MMDVFS_FINE_STEP_OPP5 (5)



/* MMCLK opps (internal used in MMDVFS) */
#define MMDVFS_MMCLK_OPP0 (0)
#define MMDVFS_MMCLK_OPP1 (1)
#define MMDVFS_MMCLK_OPP2 (2)
#define MMDVFS_MMCLK_OPP3 (3)
#define MMDVFS_MMCLK_OPP4 (4)
#define MMDVFS_MMCLK_OPP5 (5)


/* Defined the max size of MMDVFS CLK MUX table */
#define MMDVFS_CLK_MUX_NUM_LIMITATION (15)

/* CLK MUX ID */
/* Define the internal index of each MM CLK */
#define MMDVFS_CLK_MUX_TOP_SMI0_2X_SEL  (0)
#define MMDVFS_CLK_MUX_TOP_MM_SEL   (1)
#define MMDVFS_CLK_MUX_TOP_CAM_SEL  (2)
#define MMDVFS_CLK_MUX_TOP_IMG_SEL  (3)
#define MMDVFS_CLK_MUX_TOP_VENC_SEL (4)
#define MMDVFS_CLK_MUX_TOP_VDEC_SEL (5)
#define MMDVFS_CLK_MUX_TOP_MJC_SEL  (6)
#define MMDVFS_CLK_MUX_TOP_DSP_SEL  (7)
#define MMDVFS_CLK_MUX_TOP_VPU_IF_SEL  (8)
#define MMDVFS_CLK_MUX_TOP_VPU_SEL  (9)
#define MMDVFS_CLK_MUX_NUM (10)

/* Configuration Header */
struct mmdvfs_cam_property {
	int sensor_size;
	int feature_flag;
	int fps;
	int preview_size;
};

struct mmdvfs_video_property {
	int width;
	int height;
	int codec;
};

struct mmdvfs_profile {
	const char *profile_name;
	int smi_scenario_id;
	struct mmdvfs_cam_property cam_limit;
	struct mmdvfs_video_property video_limit;
};

struct mmdvfs_profile_mask {
	const char *profile_name;
	int smi_scenario_id;
	int mask_opp;
};

/* HW Configuration Management */

struct mmdvfs_clk_desc {
	void *ccf_handle;
	char *ccf_name; /* For debugging*/
};

struct mmdvfs_clk_source_desc {
	struct clk *ccf_handle;
	char *ccf_name;
	u32 clk_rate_mhz;
};

struct mmdvfs_clk_hw_map {
	int config_method; /* 0: don't set, 1: clk_mux, 2: pll hopping */
	struct mmdvfs_clk_desc clk_mux;
	int pll_id;
	int total_step;
	/* DSS value of each step for this clk */
	int step_pll_freq_map[MMDVFS_CLK_OPP_NUM_LIMITATION];
	/* CLK MUX of each step for this clk */
	int step_clk_source_id_map[MMDVFS_CLK_OPP_NUM_LIMITATION];
};

/* Record vcore and step of clks to be requested for a single MMDVFS step */
struct mmdvfs_hw_configurtion {
	int vcore_step;
	int clk_steps[MMDVFS_CLK_MUX_NUM_LIMITATION];
	int total_clks;
};

/* For a single mmdvfs step's profiles and associated hardware configuration */
struct mmdvfs_step_profile {
	int mmdvfs_step;
	struct mmdvfs_profile *profiles;
	int total_profiles;
	struct mmdvfs_hw_configurtion hw_config;
};

/* For a single mmdvfs step's profiles and associated hardware configuration */
struct mmdvfs_step_to_qos_step {
	int mmdvfs_step;
	int qos_step;
};

/* For VPU DVFS configuration */
struct mmdvfs_vpu_steps_setting {
	int vpu_dvfs_step;
	char *vpu_dvfs_step_desc;
	int mmdvfs_step;    /* Associated mmdvfs step */
	int vpu_clk_step;
	int vpu_if_clk_step;
	int vimvo_vol_step;
};

struct mmdvfs_adaptor {
	int vcore_kicker;
	int enable_vcore;
	int enable_clk_mux;
	int enable_pll;
	struct mmdvfs_clk_source_desc *mmdvfs_clk_sources;
	int mmdvfs_clk_sources_num;
	struct mmdvfs_clk_hw_map *mmdvfs_clk_hw_maps;
	int mmdvfs_clk_hw_maps_num;
	struct mmdvfs_step_profile *step_profile_mappings;
	int step_num;
	int disable_control_mask;
	void (*profile_dump)(struct mmdvfs_adaptor *self);
	void (*single_hw_config_dump)(struct mmdvfs_adaptor *self,
		struct mmdvfs_hw_configurtion *hw_configuration);
	void (*hw_config_dump)(struct mmdvfs_adaptor *self);
	int (*determine_mmdvfs_step)(struct mmdvfs_adaptor *self,
		int smi_scenario, struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting);
	int (*apply_hw_config)(struct mmdvfs_adaptor *self,
		int mmdvfs_step, int current_step);
	int (*apply_vcore_hw_config)(struct mmdvfs_adaptor *self,
		int mmdvfs_step);
	int (*apply_clk_hw_config)(struct mmdvfs_adaptor *self,
		int mmdvfs_step);
	int (*get_cam_sys_clk)(struct mmdvfs_adaptor *self, int mmdvfs_step);
	/* static members */
	void (*single_profile_dump_func)(struct mmdvfs_profile *profile);

};


struct mmdvfs_step_util {
	/* Record each scenaio's opp */
	int mmdvfs_scenario[MMDVFS_SCEN_COUNT];
	int total_scenario;
	/* Record the currency status of each opps*/
	int mmdvfs_concurrency[MMDVFS_OPP_NUM_LIMITATION];
	int total_opps;
	int wfd_vp_mix_step;
	void (*init)(struct mmdvfs_step_util *self);
	int (*set_step)(struct mmdvfs_step_util *self, s32 step, u32 scenario);
};

struct mmdvfs_vpu_dvfs_configurator {
	int nr_vpu_steps;
	int nr_vpu_clk_steps;
	int nr_vpu_if_clk_steps;
	int nr_vpu_vimvo_steps;
	struct mmdvfs_vpu_steps_setting *mmdvfs_vpu_steps_settings;

	const struct mmdvfs_vpu_steps_setting*(*get_vpu_setting)(
		struct mmdvfs_vpu_dvfs_configurator *self, int vpu_opp);
};

#define MMDVFS_PM_QOS_SUB_SYS_NUM 1
#define MMDVFS_PM_QOS_SUB_SYS_CAMERA 0

struct mmdvfs_thres_setting {
		int class_id;
		int *thresholds;
		int *opps;
		int thresholds_num;
		int mmdvfs_client_id;
};

struct mmdvfs_thres_handler {
		struct mmdvfs_thres_setting *threshold_settings;
		int mmdvfs_threshold_setting_num;
		int (*get_step)(struct mmdvfs_thres_handler *self,
			u32 class_id, u32 value);
};


extern struct mmdvfs_vpu_dvfs_configurator *g_mmdvfs_vpu_adaptor;
extern struct mmdvfs_adaptor *g_mmdvfs_adaptor;
extern struct mmdvfs_adaptor *g_mmdvfs_non_force_adaptor;
extern struct mmdvfs_step_util *g_mmdvfs_step_util;
extern struct mmdvfs_step_util *g_mmdvfs_non_force_step_util;

extern struct mmdvfs_thres_handler *g_mmdvfs_threshandler;
extern u32 camera_bw_config;

void mmdvfs_config_util_init(void);

/* #ifdef MMDVFS_QOS_SUPPORT */
#if 1
void mmdvfs_qos_update(struct mmdvfs_step_util *step_util, int new_step);
int set_qos_scenario(const char *val, const struct kernel_param *kp);
int get_qos_scenario(char *buf, const struct kernel_param *kp);
#endif
#endif /* __MMDVFS_CONFIG_UTIL_H__ */
