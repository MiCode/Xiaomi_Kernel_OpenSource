/*
 * Copyright (C) 2015	MediaTek Inc.
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
#include <linux/uaccess.h>
#ifdef VCORE_READY
#include <mtk_vcorefs_manager.h>
#else
#define KIR_MM 0
#define OPP_UNREQ -1
#endif
#include <mt-plat/mtk_chip.h>
#include <mtk_dramc.h>

#if defined(SMI_VIN)
#include "mmdvfs_config_mt6758.h"
#elif defined(SMI_CER)
#include "mmdvfs_config_mt6765.h"
#endif

#include "mmdvfs_mgr.h"
#include "mmdvfs_internal.h"
#ifdef PLL_HOPPING_READY
#include "mach/mtk_freqhopping.h"
#endif

#ifdef USE_DDR_TYPE
#include "mt_emi_api.h"
#endif
#include "mmdvfs_pmqos.h"

/* Class: mmdvfs_step_util */
static void mmdvfs_step_util_init(struct mmdvfs_step_util *self);
static int mmdvfs_step_util_set_step(
	struct mmdvfs_step_util *self, s32 step, u32 scenario);
static bool in_camera_scenario;
u32 camera_bw_config;
u32 normal_bw_config;
#if defined(SMI_VIN)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6758 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6758_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_step_util_set_step,
};

#elif defined(SMI_CER)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6765 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6765_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_step_util_set_step,
};
#endif

/* Class: mmdvfs_adaptor */
static void mmdvfs_single_profile_dump(
	struct mmdvfs_profile *profile);
static void mmdvfs_profile_dump(struct mmdvfs_adaptor *self);
static void mmdvfs_single_hw_config_dump(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_configuration);
static void mmdvfs_hw_config_dump(struct mmdvfs_adaptor *self);
static int mmdvfs_determine_step(struct mmdvfs_adaptor *self, int smi_scenario,
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting);
static int mmdvfs_apply_hw_config(
	struct mmdvfs_adaptor *self, int mmdvfs_step, int current_step);
static int mmdvfs_apply_vcore_hw_config(
	struct mmdvfs_adaptor *self, int mmdvfs_step);
static int mmdvfs_apply_clk_hw_config(
	struct mmdvfs_adaptor *self, int mmdvfs_step);
static int mmdvfs_get_cam_sys_clk(
	struct mmdvfs_adaptor *self, int mmdvfs_step);
static int check_camera_profile(struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_cam_property *profile_property);

static int check_video_profile(
	struct mmdvfs_video_property *video_setting,
	struct mmdvfs_video_property *profile_property);

#if defined(SMI_VIN)
struct mmdvfs_adaptor mmdvfs_adaptor_obj_mt6758 = {
	KIR_MM,
	0, 0, 0,
	mt6758_clk_sources, MT6758_CLK_SOURCE_NUM,
	mt6758_clk_hw_map_setting, MMDVFS_CLK_MUX_NUM,
	mt6758_step_profile, MT6758_MMDVFS_OPP_MAX,
	MT6758_MMDVFS_USER_CONTROL_SCEN_MASK,
	mmdvfs_profile_dump,
	mmdvfs_hw_config_dump,
	mmdvfs_determine_step,
	mmdvfs_apply_hw_config,
	mmdvfs_apply_vcore_hw_config,
	mmdvfs_apply_clk_hw_config,
	mmdvfs_get_cam_sys_clk,
	mmdvfs_single_profile_dump,
};

#elif defined(SMI_CER)
struct mmdvfs_adaptor mmdvfs_adaptor_obj_mt6765 = {
	KIR_MM,
	0, 0, 0,
	mt6765_clk_sources, MT6765_CLK_SOURCE_NUM,
	mt6765_clk_hw_map_setting, MMDVFS_CLK_MUX_NUM,
	mt6765_step_profile, MT6765_MMDVFS_OPP_MAX,
	MT6765_MMDVFS_USER_CONTROL_SCEN_MASK,
	mmdvfs_profile_dump,
	mmdvfs_single_hw_config_dump,
	mmdvfs_hw_config_dump,
	mmdvfs_determine_step,
	mmdvfs_apply_hw_config,
	mmdvfs_apply_vcore_hw_config,
	mmdvfs_apply_clk_hw_config,
	mmdvfs_get_cam_sys_clk,
	mmdvfs_single_profile_dump,
};

struct mmdvfs_adaptor mmdvfs_adaptor_obj_mt6765_lp3 = {
	KIR_MM,
	0, 0, 0,
	mt6765_clk_sources, MT6765_CLK_SOURCE_NUM,
	mt6765_clk_hw_map_setting, MMDVFS_CLK_MUX_NUM,
	mt6765_step_profile_lp3, MT6765_MMDVFS_OPP_MAX,
	MT6765_MMDVFS_USER_CONTROL_SCEN_MASK,
	mmdvfs_profile_dump,
	mmdvfs_single_hw_config_dump,
	mmdvfs_hw_config_dump,
	mmdvfs_determine_step,
	mmdvfs_apply_hw_config,
	mmdvfs_apply_vcore_hw_config,
	mmdvfs_apply_clk_hw_config,
	mmdvfs_get_cam_sys_clk,
	mmdvfs_single_profile_dump,
};
#endif

/* class: ISP PMQoS Handler */

/* ISP DVFS Adaptor Impementation */
static s32 get_step_by_threshold(struct mmdvfs_thres_handler *self,
	u32 class_id, u32 value);

struct mmdvfs_thres_handler mmdvfs_thres_handler_obj = {
	NULL, 0, get_step_by_threshold
};

#if defined(SMI_VIN)
struct mmdvfs_thres_handler mmdvfs_thres_handler_mt6758 = {
	mt6758_thres_handler,
	MMDVFS_PM_QOS_SUB_SYS_NUM,
	get_step_by_threshold
};

#elif defined(SMI_CER)
struct mmdvfs_thres_handler mmdvfs_thres_handler_mt6765 = {
	mt6765_thres_handler,
	MMDVFS_PM_QOS_SUB_SYS_NUM,
	get_step_by_threshold
};
#endif

/* Member function implementation */
static int mmdvfs_apply_hw_config(struct mmdvfs_adaptor *self,
	int mmdvfs_step, const int current_step)
{
	MMDVFSDEBUG(3, "current = %d, target = %d\n",
		current_step, mmdvfs_step);

	if (mmdvfs_step == current_step) {
		MMDVFSDEBUG(3, "Doesn't change step: %d\n", mmdvfs_step);
		return 0;
	}
	if (current_step == -1 ||
		((mmdvfs_step != -1) && (mmdvfs_step < current_step))) {
		if (self->apply_vcore_hw_config) {
			MMDVFSDEBUG(3, "Apply Vcore (%d -> %d):\n",
				current_step, mmdvfs_step);
			self->apply_vcore_hw_config(self, mmdvfs_step);
		} else {
			MMDVFSDEBUG(3, "vcore_hw_config is NULL.\n");
		}

		if (self->apply_clk_hw_config) {
			MMDVFSDEBUG(3, "Apply CLK setting:\n");
			self->apply_clk_hw_config(self, mmdvfs_step);
		} else {
			MMDVFSDEBUG(3, "clk_hw_config is NULL.\n");
		}
	} else {
		if (self->apply_clk_hw_config) {
			MMDVFSDEBUG(3, "Apply CLK setting:\n");
			self->apply_clk_hw_config(self, mmdvfs_step);
		} else {
			MMDVFSDEBUG(3, "clk_hw_config is NULL.\n");
		}

		if (self->apply_vcore_hw_config) {
			MMDVFSDEBUG(3, "Apply Vcore (%d -> %d)\n",
				current_step, mmdvfs_step);
			self->apply_vcore_hw_config(self, mmdvfs_step);
		} else {
			MMDVFSDEBUG(3, "vcore_hw_config is NULL.\n");
		}
	}

	return 0;
}

static int mmdvfs_apply_vcore_hw_config(
	struct mmdvfs_adaptor *self, int mmdvfs_step)
{
#ifdef VCORE_READY
	struct mmdvfs_hw_configurtion *hw_config_ptr = NULL;
	int vcore_step = 0;

	if (!self) {
		MMDVFSMSG("apply_vcore_hw_config: self can't be NULL\n");
		return -1;
	}

	if (self->enable_vcore == 0) {
		MMDVFSMSG("vcore module is disable in MMDVFS\n");
		return -1;
	}

	/* Check unrequest step */
	if (mmdvfs_step == MMDVFS_FINE_STEP_UNREQUEST) {
		vcore_step = OPP_UNREQ;
	} else {
		/* Check if the step is legall */
		if (mmdvfs_step < 0 || mmdvfs_step >= self->step_num
		|| (self->step_profile_mappings + mmdvfs_step) == NULL)
			return -1;

		/* Get hw configurtion fot the step */
		hw_config_ptr =
			&(self->step_profile_mappings + mmdvfs_step)->hw_config;

		/* Check if mmdvfs_hw_configurtion is found */
		if (hw_config_ptr == NULL)
			return -1;

		/* Get vcore step */
		vcore_step = hw_config_ptr->vcore_step;
	}

	if (vcorefs_request_dvfs_opp(self->vcore_kicker, vcore_step) != 0)
		MMDVFSMSG("Set vcore step failed: %d\n", vcore_step);
	/* Set vcore step */
	MMDVFSDEBUG(3, "Set vcore step: %d\n", vcore_step);

	return 0;
#else
	MMDVFSMSG("VCore is not ready\n");
	return -1;
#endif
}

static int mmdvfs_apply_clk_mux_config(
	struct mmdvfs_adaptor *self,
	struct mmdvfs_clk_hw_map *clk_hw_map_ptr,
	int mmdvfs_step, int clk_step)
{
	int ccf_ret = -1;
	int source_id = clk_hw_map_ptr->step_clk_source_id_map[clk_step];
	struct clk *clk_mux, *clk_source;

	if (source_id < 0 || source_id >= self->mmdvfs_clk_sources_num) {
		MMDVFSDEBUG(3, "invalid clk source id: %d, step:%d, mux:%s\n",
			source_id, mmdvfs_step,
			clk_hw_map_ptr->clk_mux.ccf_name);
		return -1;
	}

	clk_mux = clk_hw_map_ptr->clk_mux.ccf_handle;
	clk_source = self->mmdvfs_clk_sources[source_id].ccf_handle;

	MMDVFSDEBUG(3, "Change %s source to %s, expect clk = %d\n",
	clk_hw_map_ptr->clk_mux.ccf_name,
	self->mmdvfs_clk_sources[source_id].ccf_name,
	self->mmdvfs_clk_sources[source_id].clk_rate_mhz);

	if (!clk_hw_map_ptr->clk_mux.ccf_handle ||
		!self->mmdvfs_clk_sources[source_id].ccf_handle) {
		MMDVFSMSG("CCF handle can't be NULL during MMDVFS\n");
		return ccf_ret;
	}

	ccf_ret = clk_prepare_enable(clk_mux);
	MMDVFSDEBUG(3, "clk_prepare_enable: andle = %lx\n",
		(unsigned long)clk_mux);

	if (ccf_ret)
		MMDVFSMSG("prepare clk NG: %s\n",
			clk_hw_map_ptr->clk_mux.ccf_name);

	ccf_ret = clk_set_parent(clk_mux, clk_source);
	MMDVFSMSG("clk_set_parent: handle = (%lx,%lx), src id = %d\n",
		((unsigned long)clk_mux), ((unsigned long)clk_source),
		source_id);


	if (ccf_ret)
		MMDVFSMSG("Failed to set parent:%s,%s\n",
			clk_hw_map_ptr->clk_mux.ccf_name,
			self->mmdvfs_clk_sources[source_id].ccf_name);

	clk_disable_unprepare(clk_mux);
	MMDVFSDEBUG(3, "clk_disable_unprepare: handle = %lx\n",
		(unsigned long)clk_mux);
	return ccf_ret;
}

static int mmdvfs_apply_clk_pll_config(s32 pll_id, s32 clk_rate)
{
	int hopping_ret = -1;

	if (pll_id == -1) {
		MMDVFSMSG("not support PLL set rate\n");
		return -1;
	}
#ifdef PLL_HOPPING_READY
	hopping_ret = mt_dfs_general_pll(pll_id, clk_rate);
	MMDVFSMSG("pll_hopping: id = (%d), dds = 0x%08x\n", pll_id, clk_rate);

	if (hopping_ret)
		MMDVFSMSG("Failed to hopping:%d->0x%08x\n", pll_id, clk_rate);
	return hopping_ret;
#else
	MMDVFSMSG("not support PLL hopping\n");
	return hopping_ret;
#endif
}
static int mmdvfs_apply_clk_hw_config(
	struct mmdvfs_adaptor *self, int mmdvfs_step_request)
{
	struct mmdvfs_hw_configurtion *hw_config_ptr = NULL;
	int clk_idx = 0;
	int mmdvfs_step = -1;
	int clk_mux_mask = get_mmdvfs_clk_mux_mask();

	if (self->enable_clk_mux == 0) {
		MMDVFSMSG("clk_mux module is disable in MMDVFS\n");
		return -1;
	}

	if (clk_mux_mask == 0) {
		MMDVFSMSG("clk_mux_mask is 0 in MMDVFS\n");
		return -1;
	}

	/* Check unrequest step and reset the mmdvfs step to the lowest one */
	if (mmdvfs_step_request == -1)
		mmdvfs_step = self->step_num - 1;
	else
		mmdvfs_step = mmdvfs_step_request;

	/* Check if the step is legall */
	if (mmdvfs_step < 0 || mmdvfs_step >= self->step_num
	|| (self->step_profile_mappings + mmdvfs_step) == NULL)
		return -1;

	/* Get hw configurtion fot the step */
	hw_config_ptr = &(self->step_profile_mappings + mmdvfs_step)->hw_config;

	/* Check if mmdvfs_hw_configurtion is found */
	if (hw_config_ptr == NULL)
		return -1;

	MMDVFSDEBUG(3, "CLK SWITCH: total = %d, step: (%d)\n",
		hw_config_ptr->total_clks, mmdvfs_step);

	/* Get each clk and setp it accord to config method   */
	for (clk_idx = 0; clk_idx < hw_config_ptr->total_clks; clk_idx++) {
		/* Get the clk step setting of each mm clks */
		int clk_step = hw_config_ptr->clk_steps[clk_idx];
		/* Get the specific clk descriptor */
		struct mmdvfs_clk_hw_map *clk_hw_map =
			&(self->mmdvfs_clk_hw_maps[clk_idx]);

		if (clk_step < 0 || clk_step >= clk_hw_map->total_step) {
			MMDVFSDEBUG(3, "error clk step(%d) for %s\n", clk_step,
				clk_hw_map->clk_mux.ccf_name);
			continue;
		}
		if (!((1 << clk_idx) & clk_mux_mask)) {
			MMDVFSMSG("CLK %d(%s) swich is disable, mask(0x%x)\n",
			clk_idx, clk_hw_map->clk_mux.ccf_name, clk_mux_mask);
			continue;
		}
		/* Apply the clk setting, only support mux now */
		if (clk_hw_map->config_method == MMDVFS_CLK_BY_MUX) {
			mmdvfs_apply_clk_mux_config(self, clk_hw_map,
				mmdvfs_step, clk_step);
		} else if (clk_hw_map->config_method == MMDVFS_CLK_PLL_RATE) {
			mmdvfs_apply_clk_pll_config(
				clk_hw_map->pll_id,
				clk_hw_map->step_pll_freq_map[clk_step]);
		}
	}
	return 0;

}


static int mmdvfs_get_cam_sys_clk(
	struct mmdvfs_adaptor *self, int mmdvfs_step)
{
	struct mmdvfs_hw_configurtion *hw_config =
		&self->step_profile_mappings[mmdvfs_step].hw_config;
	return hw_config->clk_steps[MMDVFS_CLK_MUX_TOP_CAM_SEL];
}

/* single_profile_dump_func: */
static void mmdvfs_single_profile_dump(struct mmdvfs_profile *profile)
{
	if (profile == NULL) {
		MMDVFSDEBUG(3, "_dump: NULL profile found\n");
		return;
	}
	MMDVFSDEBUG(3, "%s, %d, (%d,%d,0x%x,%d), (%d,%d,%d)\n",
	profile->profile_name, profile->smi_scenario_id,
	profile->cam_limit.sensor_size, profile->cam_limit.feature_flag,
	profile->cam_limit.fps, profile->cam_limit.preview_size,
	profile->video_limit.width, profile->video_limit.height,
	profile->video_limit.codec);
}

/* Profile Matching Util */
static void mmdvfs_profile_dump(struct mmdvfs_adaptor *self)
{
	int i =	0;

	struct mmdvfs_step_profile *profile_mapping =
	self->step_profile_mappings;

	if (profile_mapping == NULL) {
		MMDVFSMSG("dump: step_profile_mappings can't be NULL\n");
		return;
	}

	MMDVFSDEBUG(3, "MMDVFS DUMP (%d):\n", profile_mapping->mmdvfs_step);

	if (profile_mapping == NULL)
		MMDVFSDEBUG(3, "dump: step_profile_mappings can't be NULL\n");
	MMDVFSDEBUG(3, "MMDVFS DUMP (%d):\n", profile_mapping->mmdvfs_step);
	for (i = 0; i < profile_mapping->total_profiles; i++) {
		struct mmdvfs_profile *profile = profile_mapping->profiles + i;

		mmdvfs_single_profile_dump(profile);
	}
}

static void mmdvfs_single_hw_config_dump(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_configuration)
{
	int i = 0;
	const int clk_soure_total = self->mmdvfs_clk_sources_num;
	struct mmdvfs_clk_source_desc *clk_sources = self->mmdvfs_clk_sources;
	struct mmdvfs_clk_hw_map *clk_hw_map = self->mmdvfs_clk_hw_maps;

	if (clk_hw_map == NULL) {
		MMDVFSMSG(
		"single_hw_config_dump: mmdvfs_clk_hw_maps can't	be NULL\n");
		return;
	}

	if (hw_configuration == NULL) {
		MMDVFSMSG(
		"single_hw_config_dump: hw_configuration can't be NULL\n");
		return;
	}

	MMDVFSDEBUG(3, "Vcore tep: %d\n", hw_configuration->vcore_step);

	for (i = 0; i < hw_configuration->total_clks; i++) {
		char *ccf_clk_source_name = "NONE";
		char *ccf_clk_mux_name = "NONE";
		u32 clk_rate_mhz = 0;
		u32 clk_step = 0;
		u32 clk_source = 0;

		struct mmdvfs_clk_hw_map *map_item = clk_hw_map + i;

		clk_step = hw_configuration->clk_steps[i];

		if (map_item != NULL && map_item->clk_mux.ccf_name != NULL) {
			clk_source = map_item->step_clk_source_id_map[clk_step];
			ccf_clk_mux_name = map_item->clk_mux.ccf_name;
		}

		if (map_item->config_method == MMDVFS_CLK_BY_MUX
			&& clk_source < clk_soure_total) {
			ccf_clk_source_name = clk_sources[clk_source].ccf_name;
			clk_rate_mhz = clk_sources[clk_source].clk_rate_mhz;
		}

		MMDVFSDEBUG(3, "\t%s, %s, %dMhz\n", map_item->clk_mux.ccf_name,
		ccf_clk_source_name, clk_rate_mhz);
	}
}

static bool mmdvfs_determine_step_impl(
	struct mmdvfs_step_profile *mapping_ptr,
	int smi_scenario,
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting)
{
	int index = 0;

	for (index = 0; index < mapping_ptr->total_profiles; index++) {
		/* Check if the scenario matches any profile */
		struct mmdvfs_profile *profile =
			mapping_ptr->profiles + index;

		if (smi_scenario != profile->smi_scenario_id)
			continue;

		/* Check cam setting */
		if (!check_camera_profile(cam_setting, &profile->cam_limit))
		/* Doesn't match the camera property condition, skip this run */
			continue;
		if (!check_video_profile(codec_setting, &profile->video_limit))
		/* Doesn't match the video property condition, skip this run */
			continue;
		/* Complete match, return the opp index */
		mmdvfs_single_profile_dump(profile);
		return true;
	}
	return false;
}

static int mmdvfs_determine_step(struct mmdvfs_adaptor *self,
	int smi_scenario,
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting)
{
	/* Find the matching scenario from OPP 0 to Max OPP */
	int opp_index =	0;
	struct mmdvfs_step_profile *profile_mappings =
	self->step_profile_mappings;
	const int opp_max_num =	self->step_num;

	if (profile_mappings == NULL) {
		MMDVFSMSG(
		"mmdvfs_mmdvfs_step: step_profile_mappings can't be NULL\n");
		return MMDVFS_FINE_STEP_UNREQUEST;
	}

	for (opp_index = 0; opp_index < opp_max_num; opp_index++) {
		struct mmdvfs_step_profile *mapping_ptr =
			profile_mappings + opp_index;
		if (mmdvfs_determine_step_impl(mapping_ptr, smi_scenario,
			cam_setting, codec_setting))
			return opp_index;
	}

	/* If there is no profile matched, return -1 (no dvfs request)*/
	return MMDVFS_FINE_STEP_UNREQUEST;
}

/* Show each setting of opp */
static void mmdvfs_hw_config_dump(struct mmdvfs_adaptor *self)
{
	int i =	0;
	struct mmdvfs_step_profile *mapping =
	self->step_profile_mappings;

	if (mapping == NULL) {
		MMDVFSMSG(
		"mmdvfs_hw_config_dump: mmdvfs_clk_hw_maps can't	be NULL");
		return;
	}

	MMDVFSDEBUG(3, "All OPP	configurtion dump\n");
	for (i = 0; i < self->step_num; i++) {
		struct mmdvfs_step_profile *mapping_item = mapping + i;

		MMDVFSDEBUG(3, "MMDVFS OPP %d:\n", i);
		if (mapping_item != NULL)
			self->single_hw_config_dump(
				self, &mapping_item->hw_config);
	}
}

static int check_camera_profile(
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_cam_property *profile_property)
{
	int is_match = 1;

	/* Null pointer check: */
	/* If the scenario doesn't has cam_setting, then there */
	/* is no need to check the cam property */
	if (!cam_setting || !profile_property) {
		is_match = 1;
	} else {
		int default_feature_flag = cam_setting->feature_flag
			& (~(MMDVFS_CAMERA_MODE_FLAG_DEFAULT));

		/* Check the minium sensor resolution */
		if (cam_setting->sensor_size < profile_property->sensor_size)
			is_match = 0;

		/* Check the minium sensor resolution */
		if (cam_setting->fps < profile_property->fps)
			is_match = 0;

		/* Check the minium sensor resolution */
		if (cam_setting->preview_size < profile_property->preview_size)
			is_match = 0;

		/* Check the if the feature match */
		/* Not match if there is no featue matching the	profile's one */
		/* 1 ==> don't change */
		/* 0 ==> set is_match to 0 */
		if (profile_property->feature_flag != 0
		&& !(default_feature_flag & profile_property->feature_flag))
			is_match = 0;
	}

	return is_match;
}

static int check_video_profile(
	struct mmdvfs_video_property *video_setting,
	struct mmdvfs_video_property *profile_property)
{
	int is_match = 1;
	/* Null pointer check: */
	/* If the scenario doesn't has video_setting, then there */
	/* is no need to check the video property */

	if (!video_setting || !profile_property)
		is_match = 1;
	else {
		if (!(video_setting->height * video_setting->width
		>= profile_property->height * profile_property->width))
			/* Check the minium sensor resolution */
			is_match = 0;
	}

	return is_match;
}

static void mmdvfs_step_util_init(struct mmdvfs_step_util *self)
{
	int idx	= 0;

	for (idx = 0; idx < self->total_scenario; idx++)
		self->mmdvfs_scenario[idx] = MMDVFS_FINE_STEP_UNREQUEST;

	for (idx = 0; idx < self->total_opps; idx++)
		self->mmdvfs_concurrency[idx] = 0;
}

static inline void mmdvfs_adjust_scenario(s32 *mmdvfs_scen_opp_map,
	u32 changed_scenario, u32 new_scenario)
{
	if (mmdvfs_scen_opp_map[changed_scenario] >= 0) {
		mmdvfs_scen_opp_map[changed_scenario] = -1;
		MMDVFSDEBUG(5, "[adj] new scenario (%d) change (%d) to -1!\n",
			new_scenario, changed_scenario);
	}
}

#define MMDVFS_CAMERA_SCEN_MASK	((1<<SMI_BWC_SCEN_VR) | \
				(1<<SMI_BWC_SCEN_VR_SLOW) | \
				(1<<SMI_BWC_SCEN_ICFP) | \
				(1<<SMI_BWC_SCEN_VSS) | \
				(1<<SMI_BWC_SCEN_CAM_PV) | \
				(1<<SMI_BWC_SCEN_CAM_CP))

/* updat the step members only (HW independent part) */
/* return the final step */
static int mmdvfs_step_util_set_step(struct mmdvfs_step_util *self,
	s32 step, u32 scenario)
{
	int idx, opp_idx;
	int final_opp = -1;
	bool has_camera_scenario = false;

	/* check step range here */
	if (step < -1 || step >= self->total_opps)
		return MMDVFS_FINE_STEP_UNREQUEST;

	/* check invalid scenario */
	if (scenario >= self->total_scenario)
		return MMDVFS_FINE_STEP_UNREQUEST;

	self->mmdvfs_scenario[scenario] = step;

	if (self->wfd_vp_mix_step >= 0) {
		/* special configuration for mixed step */
		if (self->mmdvfs_scenario[SMI_BWC_SCEN_WFD] >= 0 &&
			self->mmdvfs_scenario[SMI_BWC_SCEN_VP] >= 0) {
			self->mmdvfs_scenario[MMDVFS_SCEN_VP_WFD] =
				self->wfd_vp_mix_step;
		} else {
			self->mmdvfs_scenario[MMDVFS_SCEN_VP_WFD] =
				MMDVFS_FINE_STEP_UNREQUEST;
		}
	}

	/* Reset the concurrency fileds before the calculation */
	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++)
		self->mmdvfs_concurrency[opp_idx] = 0;

	for (idx = 0; idx < self->total_scenario; idx++) {
		for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
			if (self->mmdvfs_scenario[idx] == opp_idx)
				self->mmdvfs_concurrency[opp_idx] |= 1 << idx;
		}
	}

	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
		if (self->mmdvfs_concurrency[opp_idx] != 0) {
			final_opp = opp_idx;
			break;
		}
	}

	if (self->mmdvfs_scenario[MMDVFS_MGR] != MMDVFS_FINE_STEP_UNREQUEST) {
		final_opp = self->mmdvfs_scenario[MMDVFS_MGR];
		MMDVFSMSG("[force] set step (%d)!\n", final_opp);
	}

	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
		if ((self->mmdvfs_concurrency[opp_idx] &
				MMDVFS_CAMERA_SCEN_MASK) != 0) {
			has_camera_scenario = true;
			break;
		}
	}

	if (camera_bw_config && in_camera_scenario != has_camera_scenario) {
#if defined(SPECIAL_BW_CONFIG_MM)
		u32 bw_config =
			has_camera_scenario ?
				camera_bw_config : normal_bw_config;
		u32 old_bw_config, new_bw_config;

		MMDVFSDEBUG(
			3, "[DRAM setting] in camera? %d\n",
			has_camera_scenario);
		in_camera_scenario = has_camera_scenario;
		old_bw_config = BM_GetBW();
		BM_SetBW(bw_config);
		new_bw_config = BM_GetBW();
		MMDVFSDEBUG(
			3,
			"[DRAM setting] old:0x%08x, want:0x%08x, new:0x%08x\n",
			old_bw_config, bw_config, new_bw_config);
#else
		MMDVFSDEBUG(3, "[DRAM setting] not support\n");
#endif
	}
	return final_opp;
}


/* ISP DVFS Adaptor Impementation */
static s32 get_step_by_threshold(struct mmdvfs_thres_handler *self,
	u32 class_id, u32 value)
{
	struct mmdvfs_thres_setting *setting = NULL;
	int step_found = MMDVFS_FINE_STEP_UNREQUEST;
	int threshold_idx = 0;

	if (class_id > self->mmdvfs_threshold_setting_num)
		return MMDVFS_FINE_STEP_UNREQUEST;

	if (value == 0)
		return MMDVFS_FINE_STEP_UNREQUEST;

	setting = &self->threshold_settings[class_id];

	for (threshold_idx = 0;
		threshold_idx < setting->thresholds_num;
		threshold_idx++) {
		int *thres = setting->thresholds + threshold_idx;
		int *opp = setting->opps + threshold_idx;

		if ((thres) && (opp) && (value >= *thres)) {
			MMDVFSDEBUG(5, "value=%d,threshold=%d\n",
				value, *thres);
			step_found = *opp;
			break;
		}
	}
	if (step_found == MMDVFS_FINE_STEP_UNREQUEST)
		step_found = *(setting->opps + setting->thresholds_num-1);

	return step_found;
}

struct mmdvfs_vpu_dvfs_configurator *g_mmdvfs_vpu_adaptor;
struct mmdvfs_adaptor *g_mmdvfs_adaptor;
struct mmdvfs_adaptor *g_mmdvfs_non_force_adaptor;
struct mmdvfs_step_util *g_mmdvfs_step_util;
struct mmdvfs_step_util *g_mmdvfs_non_force_step_util;
struct mmdvfs_thres_handler *g_mmdvfs_threshandler;

/* #ifdef MMDVFS_QOS_SUPPORT */
#if 1
static int mask_concur[MMDVFS_OPP_NUM_LIMITATION];
static void update_qos_scenario(void);
#endif
void mmdvfs_config_util_init(void)
{
	int mmdvfs_profile_id = mmdvfs_get_mmdvfs_profile();

	switch (mmdvfs_profile_id) {
	case MMDVFS_PROFILE_VIN:
#if defined(SMI_VIN)
		g_mmdvfs_adaptor = &mmdvfs_adaptor_obj_mt6758;
		g_mmdvfs_step_util = &mmdvfs_step_util_obj_mt6758;
		g_mmdvfs_threshandler = &mmdvfs_thres_handler_mt6758;
#endif
		break;
	case MMDVFS_PROFILE_CER:
#if defined(SMI_CER)
		g_mmdvfs_threshandler = &mmdvfs_thres_handler_mt6765;
#if defined(USE_DDR_TYPE)
		if (get_dram_type() == TYPE_LPDDR3) {
			g_mmdvfs_adaptor = &mmdvfs_adaptor_obj_mt6765_lp3;
			MMDVFSMSG("g_mmdvfs_step_util init with lp3\n");
		} else{
			g_mmdvfs_adaptor = &mmdvfs_adaptor_obj_mt6765;
			MMDVFSMSG("g_mmdvfs_step_util init with lp4 2-ch\n");
		}
#else
		g_mmdvfs_adaptor = &mmdvfs_adaptor_obj_mt6765;
		MMDVFSMSG("g_mmdvfs_step_util init with lp4 2-ch\n");
#endif
		g_mmdvfs_step_util = &mmdvfs_step_util_obj_mt6765;
#endif
		break;
	default:
		break;
	}

	g_mmdvfs_adaptor->profile_dump(g_mmdvfs_adaptor);
	MMDVFSMSG("g_mmdvfs_step_util init\n");

	if (g_mmdvfs_step_util)
		g_mmdvfs_step_util->init(g_mmdvfs_step_util);

	if (g_mmdvfs_non_force_step_util)
		g_mmdvfs_non_force_step_util->init(
			g_mmdvfs_non_force_step_util);

/* #ifdef MMDVFS_QOS_SUPPORT */
#if 1
	update_qos_scenario();
#endif
}

void mmdvfs_pm_qos_update_request(
	struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 new_value)
{
	int step = MMDVFS_FINE_STEP_UNREQUEST;
	int mmdvfs_vote_id = -1;
	struct mmdvfs_thres_setting *client_threshold_setting = NULL;

	if (!req) {
		MMDVFSDEBUG(5, "qos_update: no request\n");
		return;
	}

	if (!g_mmdvfs_threshandler) {
		MMDVFSMSG("qos_update: handler is NULL\n");
		return;
	}

	client_threshold_setting =
		g_mmdvfs_threshandler->threshold_settings + mmdvfs_pm_qos_class;

	if (!client_threshold_setting) {
		MMDVFSMSG("qos_update: client_threshold_setting is NULL\n");
		return;
	}

	mmdvfs_vote_id = client_threshold_setting->mmdvfs_client_id;
	step = g_mmdvfs_threshandler->get_step(g_mmdvfs_threshandler,
		mmdvfs_pm_qos_class, new_value);

	mmdvfs_set_fine_step(mmdvfs_vote_id, step);
}

void mmdvfs_pm_qos_remove_request(struct mmdvfs_pm_qos_request *req)
{
	if (!req) {
		MMDVFSDEBUG(5, "qos_remove: req is NULL\n");
		return;
	}
	mmdvfs_pm_qos_update_request(req, req->pm_qos_class, 0);
}

void mmdvfs_pm_qos_add_request(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 value)
{
	if (!req) {
		MMDVFSDEBUG(5, "qos_add: req is NULL\n");
		return;
	}

	mmdvfs_pm_qos_update_request(req, mmdvfs_pm_qos_class, value);
}

static struct mmdvfs_thres_setting *get_threshold_setting(u32 class_id)
{
	if (class_id > g_mmdvfs_threshandler->mmdvfs_threshold_setting_num)
		return NULL;

	return &g_mmdvfs_threshandler->threshold_settings[class_id];
}

u32 mmdvfs_qos_get_thres_count(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class)
{
	struct mmdvfs_thres_setting *setting_ptr =
		get_threshold_setting(mmdvfs_pm_qos_class);

	if (setting_ptr)
		return setting_ptr->thresholds_num;

	return 0;
}

u32 mmdvfs_qos_get_thres_value(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 thres_idx)
{
	struct mmdvfs_thres_setting *setting_ptr =
		get_threshold_setting(mmdvfs_pm_qos_class);

	if (setting_ptr) {
		if (thres_idx > setting_ptr->thresholds_num)
			return 0;
		return setting_ptr->thresholds[thres_idx];
	}
	return 0;
}
#define MHZ 1000000
u32 mmdvfs_qos_get_cur_thres(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class)
{
	struct mmdvfs_clk_hw_map *hw_map_ptr = NULL;
	u32 clk_step = 0, config_method = 0;
	s32 current_step = mmdvfs_get_current_fine_step();
	struct mmdvfs_adaptor *adaptor = g_mmdvfs_adaptor;
	u32 hw_maps_index;

	if (current_step < 0)
		return 0;

	if (mmdvfs_pm_qos_class == MMDVFS_PM_QOS_SUB_SYS_CAMERA)
		hw_maps_index = MMDVFS_CLK_MUX_TOP_CAM_SEL;
	else
		return 0;

	clk_step = adaptor->get_cam_sys_clk(adaptor, current_step);
	hw_map_ptr = &adaptor->mmdvfs_clk_hw_maps[MMDVFS_CLK_MUX_TOP_CAM_SEL];
	config_method = hw_map_ptr->config_method;
	if (config_method == MMDVFS_CLK_PLL_RATE)
		return hw_map_ptr->step_pll_freq_map[clk_step] / MHZ;
	else if (config_method == MMDVFS_CLK_BY_MUX) {
		u32 source_id = hw_map_ptr->step_clk_source_id_map[clk_step];

		return adaptor->mmdvfs_clk_sources[source_id].clk_rate_mhz;
	}
	return 0;
}

/* #ifdef MMDVFS_QOS_SUPPORT */
#if 1
static int get_qos_step(s32 opp)
{
	if (opp == MMDVFS_FINE_STEP_UNREQUEST)
		return opp;
	if (opp >= ARRAY_SIZE(legacy_to_qos_step))
		return MMDVFS_FINE_STEP_UNREQUEST;

	return legacy_to_qos_step[opp].qos_step;
}

void mmdvfs_qos_update(struct mmdvfs_step_util *step_util, int new_step)
{
	int i;
	int *concur = step_util->mmdvfs_concurrency;

	i = ARRAY_SIZE(qos_apply_profiles) - 1;
	if (qos_apply_profiles[i].smi_scenario_id == QOS_ALL_SCENARIO) {
		MMDVFSDEBUG(5, "force update qos step: %d\n", new_step);
		mmdvfs_qos_force_step(get_qos_step(new_step));
		return;
	}
	for (i = 0; i < MMDVFS_OPP_NUM_LIMITATION; i++) {
		if (mask_concur[i] & concur[i]) {
			/* scenario matched, check */
			MMDVFSDEBUG(5, "qos match,S(%d,%d,0x%0x,0x%0x)\n",
				new_step, i, mask_concur[i],
				step_util->mmdvfs_concurrency[i]);
			mmdvfs_qos_force_step(get_qos_step(i));
			return;
		}
	}
	/* No scenario is matched, cancel qos step anyway */
	mmdvfs_qos_force_step(get_qos_step(MMDVFS_FINE_STEP_UNREQUEST));
}

static void update_qos_scenario(void)
{
	int i;

	memset(&mask_concur, 0, sizeof(mask_concur));
	for (i = 0; i < ARRAY_SIZE(qos_apply_profiles); i++) {
		int opp = qos_apply_profiles[i].mask_opp;

		if (opp >= MMDVFS_OPP_NUM_LIMITATION || opp < 0)
			continue;
		mask_concur[opp] |=
			(1 << qos_apply_profiles[i].smi_scenario_id);
	}
}

int set_qos_scenario(const char *val, const struct kernel_param *kp)
{
	int i, result, scenario, opp;

	result = sscanf(val, "%d %d", &scenario, &opp);
	if (result != 2) {
		MMDVFSMSG("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	if (scenario != QOS_ALL_SCENARIO &&
		(scenario < 0 || scenario > MMDVFS_SCEN_COUNT)) {
		MMDVFSMSG(
			"scenario %d not in %d~%d\n",
			scenario, 0, MMDVFS_SCEN_COUNT);
		return -EINVAL;
	}
	if (opp < MMDVFS_FINE_STEP_UNREQUEST || opp > MMDVFS_FINE_STEP_OPP5) {
		MMDVFSMSG("opp %d not in %d~%d\n", opp,
			MMDVFS_FINE_STEP_UNREQUEST, MMDVFS_FINE_STEP_OPP5);
		return -EINVAL;
	}
	/* only update latest debug entry */
	i = ARRAY_SIZE(qos_apply_profiles) - 1;
	qos_apply_profiles[i].smi_scenario_id = scenario;
	qos_apply_profiles[i].mask_opp = opp;

	update_qos_scenario();
	return 0;
}

int get_qos_scenario(char *buf, const struct kernel_param *kp)
{
	int i, off = 0;

	for (i = 0; i < ARRAY_SIZE(qos_apply_profiles); i++) {
		off += snprintf(buf + off, PAGE_SIZE - off,
			"[%d]%s: %d / %d\n", i,
			qos_apply_profiles[i].profile_name,
			qos_apply_profiles[i].smi_scenario_id,
			qos_apply_profiles[i].mask_opp);
	}
	buf[off] = '\0';
	return off;
}
#endif
