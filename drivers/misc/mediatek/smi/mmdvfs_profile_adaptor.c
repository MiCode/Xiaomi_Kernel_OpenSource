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
#include <linux/clk.h>
#include <mtk_vcorefs_manager.h>
#include <mt-plat/mtk_chip.h>
#include <mtk_dramc.h>

#if defined(SMI_WHI)
#include "mmdvfs_config_mt6799.h"
#include "mmdvfs_config_mt6799_v2.h"
#elif defined(SMI_ALA)
#include "mmdvfs_config_mt6759.h"
#elif defined(SMI_BIA)
#include "mmdvfs_config_mt6763.h"
#elif defined(SMI_VIN)
#include "mmdvfs_config_mt6758.h"
#elif defined(SMI_ZIO)
#include "mmdvfs_config_mt6739.h"
#elif defined(SMI_SYL)
#include "mmdvfs_config_mt6771.h"
#elif defined(SMI_CAN)
#include "mmdvfs_config_mt6775.h"
#endif

#include "mtk_smi.h"
#include "mmdvfs_mgr.h"
#include "mmdvfs_internal.h"

#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif

#ifdef USE_DDR_TYPE
#include "mt_emi_api.h"
#endif
#include "mmdvfs_pmqos.h"


/* Class: mmdvfs_step_util */
static int mmdvfs_get_legacy_mmclk_step_from_mmclk_opp(
	struct mmdvfs_step_util *self, int mmclk_step);
static int mmdvfs_get_opp_from_legacy_step(
	struct mmdvfs_step_util *self, int legacy_step);
static void mmdvfs_step_util_init(struct mmdvfs_step_util *self);
static int mmdvfs_step_util_set_step(
	struct mmdvfs_step_util *self, s32 step, u32 scenario);
static int mmdvfs_get_clients_clk_opp(
	struct mmdvfs_step_util *self, struct mmdvfs_adaptor *adaptor,
	int clients_mask, int clk_id);

#ifdef DYNAMIC_DISP_HRT
static void mmdvfs_change_disp_hrt(bool has_camera_scenario);
#endif
static bool in_camera_scenario;
u32 camera_bw_config;
u32 normal_bw_config;
static disp_hrt_change_cb g_disp_hrt_change_cb;
u32 cam_sensor_threshold;
u32 disp_hrt_decrease_level1;
u32 disp_hrt_decrease_default;
static bool g_is_md_on;


#if defined(SMI_WHI)
struct mmdvfs_step_util mmdvfs_step_util_obj = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MMDVFS_OPP_MAX,
	mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_UNREQUEST,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

struct mmdvfs_step_util mmdvfs_step_util_obj_mt6799_v2 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6799_V2_MMDVFS_OPP_MAX,
	mmdvfs_legacy_step_to_opp_6799v2,
	MMDVFS_VOLTAGE_COUNT,
	mmdvfs_mmclk_opp_to_legacy_mmclk_step_6799v2,
	MT6799_V2_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_UNREQUEST,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_ALA)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6759 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6759_MMDVFS_OPP_MAX,
	mt6759_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6759_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6759_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_UNREQUEST,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

struct mmdvfs_step_util mmdvfs_step_util_obj_mt6759_non_force = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6759_MMDVFS_OPP_MAX,
	mt6759_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6759_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6759_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_UNREQUEST,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_BIA)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6763 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6763_MMDVFS_OPP_MAX,
	mt6763_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6763_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6763_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_VIN)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6758 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6758_MMDVFS_OPP_MAX,
	mt6758_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6758_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6758_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_ZIO)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6739 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6739_MMDVFS_OPP_MAX,
	mt6739_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6739_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6739_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_SYL)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6771 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6771_MMDVFS_OPP_MAX,
	mt6771_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6771_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6771_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};

#elif defined(SMI_CAN)
struct mmdvfs_step_util mmdvfs_step_util_obj_mt6775 = {
	{0},
	MMDVFS_SCEN_COUNT,
	{0},
	MT6775_MMDVFS_OPP_MAX,
	mt6775_mmdvfs_legacy_step_to_opp,
	MMDVFS_VOLTAGE_COUNT,
	mt6775_mmdvfs_mmclk_opp_to_legacy_mmclk_step,
	MT6775_MMDVFS_OPP_MAX,
	MMDVFS_FINE_STEP_OPP0,
	mmdvfs_step_util_init,
	mmdvfs_get_legacy_mmclk_step_from_mmclk_opp,
	mmdvfs_get_opp_from_legacy_step,
	mmdvfs_step_util_set_step,
	mmdvfs_get_clients_clk_opp
};
#endif

/* Class: mmdvfs_adaptor */
static void mmdvfs_single_profile_dump(struct mmdvfs_profile *profile);
static void mmdvfs_profile_dump(struct mmdvfs_adaptor *self);
static void mmdvfs_single_hw_configuration_dump(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_configuration);
static void mmdvfs_hw_configuration_dump(struct mmdvfs_adaptor *self);
static int mmdvfs_determine_step(struct mmdvfs_adaptor *self,
	int smi_scenario,
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting);
static int mmdvfs_apply_hw_configurtion_by_step(struct mmdvfs_adaptor *self,
	int mmdvfs_step, int current_step);
static int mmdvfs_apply_vcore_hw_configurtion_by_step(
	struct mmdvfs_adaptor *self, int mmdvfs_step);
static int mmdvfs_apply_clk_hw_configurtion_by_step(
	struct mmdvfs_adaptor *self, int mmdvfs_step, bool to_high);
static int mmdvfs_get_cam_sys_clk(
	struct mmdvfs_adaptor *self, int mmdvfs_step);

static int is_camera_profile_matched(struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_cam_property *profile_property);

static int is_video_profile_matched(
	struct mmdvfs_video_property *video_setting,
	struct mmdvfs_video_property *profile_property);

#if defined(SMI_ZIO)
struct mmdvfs_adaptor mmdvfs_adaptor_obj_mt6739 = {
	KIR_MM,
	0, 0, 0,
	mt6739_clk_sources, MT6739_CLK_SOURCE_NUM,
	mt6739_mmdvfs_clk_hw_map, MMDVFS_CLK_MUX_NUM,
	mt6739_step_profile, MT6739_MMDVFS_OPP_MAX,
	MT6739_MMDVFS_SMI_USER_CONTROL_SCEN_MASK,
	mmdvfs_profile_dump,
	mmdvfs_single_hw_configuration_dump,
	mmdvfs_hw_configuration_dump,
	mmdvfs_determine_step,
	mmdvfs_apply_hw_configurtion_by_step,
	mmdvfs_apply_vcore_hw_configurtion_by_step,
	mmdvfs_apply_clk_hw_configurtion_by_step,
	mmdvfs_get_cam_sys_clk,
	mmdvfs_single_profile_dump,
};
#endif

/* class: ISP PMQoS Handler */

/* ISP DVFS Adaptor Impementation */
static s32 get_step_by_threshold(struct mmdvfs_thresholds_dvfs_handler *self,
	u32 class_id, u32 value);

struct mmdvfs_thresholds_dvfs_handler mmdvfs_thresholds_dvfs_handler_obj = {
	NULL,
	0,
	get_step_by_threshold
};

#if defined(SMI_ZIO)
struct mmdvfs_thresholds_dvfs_handler dvfs_handler_mt6739 = {
	mt6739_mmdvfs_threshold,
	MMDVFS_PMQOS_NUM,
	get_step_by_threshold
};
#endif

static const struct mmdvfs_vpu_steps_setting *get_vpu_setting_impl(
	struct mmdvfs_vpu_dvfs_configurator *self, int vpu_opp);

struct mmdvfs_vpu_dvfs_configurator mmdvfs_vpu_dvfs_configurator_obj = {
	0,
	0,
	0,
	0,
	NULL,
	get_vpu_setting_impl
};

/* Member function implementation */
static int mmdvfs_apply_hw_configurtion_by_step(struct mmdvfs_adaptor *self,
	int mmdvfs_step, const int current_step)
{
	MMDVFSDEBUG(3, "current = %d, target = %d\n",
		current_step, mmdvfs_step);

	if (mmdvfs_step == current_step) {
		MMDVFSDEBUG(3, "Doesn't change step, already in step: %d\n",
			mmdvfs_step);
	} else {
		if (current_step == -1 || ((mmdvfs_step != -1)
			&& (mmdvfs_step < current_step))) {
			if (self->apply_vcore_hw_configurtion_by_step) {
				MMDVFSDEBUG(3, "Apply setting(%d --> %d):\n",
					current_step, mmdvfs_step);
				MMDVFSDEBUG(3, "current = %d, target = %d\n",
					current_step, mmdvfs_step);
				self->apply_vcore_hw_configurtion_by_step(
					self, mmdvfs_step);
			} else {
				MMDVFSDEBUG(3, "apply_vcore NULL.\n");
			}

			if (self->apply_clk_hw_configurtion_by_step) {
				MMDVFSDEBUG(3, "Apply CLK setting:\n");
				self->apply_clk_hw_configurtion_by_step(
					self, mmdvfs_step, true);
			} else {
				MMDVFSDEBUG(3, "apply_clk NULL.\n");
			}
		} else {
			if (self->apply_clk_hw_configurtion_by_step) {
				MMDVFSDEBUG(3, "Apply CLK setting:\n");
				self->apply_clk_hw_configurtion_by_step(
					self, mmdvfs_step, false);
			} else {
				MMDVFSDEBUG(3, "apply_clk NULL.\n");
			}

			if (self->apply_vcore_hw_configurtion_by_step) {
				MMDVFSDEBUG(3, "Apply setting(%d --> %d)\n",
					current_step, mmdvfs_step);
				MMDVFSDEBUG(3, "current = %d, target = %d\n",
					current_step, mmdvfs_step);
				self->apply_vcore_hw_configurtion_by_step(
					self, mmdvfs_step);
			} else {
				MMDVFSDEBUG(3, "apply_vcore NULL.\n");
			}
		}
	}

	return 0;
}

static int mmdvfs_apply_vcore_hw_configurtion_by_step(
	struct mmdvfs_adaptor *self, int mmdvfs_step)
{
	struct mmdvfs_hw_configurtion *hw_config_ptr = NULL;
	int vcore_step = 0;

	if (!self) {
		MMDVFSMSG("apply_vcore: self can't be NULL\n");
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

}

static void mmdvfs_configure_clk_hw(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_config_ptr,
	int clk_idx, int mmdvfs_step)
{
	/* Get the clk step setting of each mm clks */
	int clk_step = hw_config_ptr->clk_steps[clk_idx];
	/* Get the specific clk descriptor */
	struct mmdvfs_clk_hw_map *clk_hw_map =
		&(self->mmdvfs_clk_hw_maps[clk_idx]);
	int clk_mux_mask = get_mmdvfs_clk_mux_mask();

	if (clk_step < 0 || clk_step >= clk_hw_map->total_step) {
		MMDVFSDEBUG(3, "invalid clk step (%d) for %s\n", clk_step,
		clk_hw_map->clk_mux.ccf_name);
		return;
	}
	if (!((1 << clk_idx) & clk_mux_mask)) {
		MMDVFSMSG("CLK %d(%s) swich is not enabled, mask(0x%x)\n",
		clk_idx, clk_hw_map->clk_mux.ccf_name, clk_mux_mask);
		return;
	}
	/* Apply the clk setting */
	if (clk_hw_map->config_method == MMDVFS_CLK_CONFIG_BY_MUX) {
		/* Get clk source */
		int clk_id =
			clk_hw_map->step_clk_source_id_map[clk_step];
		int ccf_ret = -1;

		if (clk_id < 0
			|| clk_id >= self->mmdvfs_clk_sources_num) {
			MMDVFSDEBUG(3, "invalid source: %d step:%d mux:%s\n",
				clk_id, mmdvfs_step,
				clk_hw_map->clk_mux.ccf_name);
			return;
		}
		MMDVFSDEBUG(3, "Change %s source to %s, expect clk = %d\n",
		clk_hw_map->clk_mux.ccf_name,
		self->mmdvfs_clk_sources[clk_id].ccf_name,
		self->mmdvfs_clk_sources[clk_id].clk_rate_mhz);

		if (self->mmdvfs_clk_sources[clk_id].ccf_handle == NULL
			|| clk_hw_map->clk_mux.ccf_handle == NULL) {
			MMDVFSMSG("CCF handle can't be NULL during MMDVFS\n");
			return;
		}

		ccf_ret = clk_prepare_enable(
			(struct clk *)clk_hw_map->clk_mux.ccf_handle);
		MMDVFSDEBUG(3, "clk_prepare_enable: andle = %lx\n",
		((unsigned long)clk_hw_map->clk_mux.ccf_handle));

		if (ccf_ret)
			MMDVFSMSG("prepare clk NG: %s\n",
				clk_hw_map->clk_mux.ccf_name);

		ccf_ret = clk_set_parent(
		(struct clk *)clk_hw_map->clk_mux.ccf_handle,
		(struct clk *)self->mmdvfs_clk_sources[clk_id].ccf_handle);
		MMDVFSDEBUG(3, "clk_set_parent (%lx,%lx), src id = %d\n",
		((unsigned long)clk_hw_map->clk_mux.ccf_handle),
		((unsigned long)self->mmdvfs_clk_sources[clk_id].ccf_handle),
		clk_id);

		if (ccf_ret)
			MMDVFSMSG("Failed to set parent:%s,%s\n",
			clk_hw_map->clk_mux.ccf_name,
			self->mmdvfs_clk_sources[clk_id].ccf_name);

		clk_disable_unprepare(
			(struct clk *)clk_hw_map->clk_mux.ccf_handle);
		MMDVFSDEBUG(3, "clk_disable_unprepare: handle = %lx\n",
		((unsigned long)clk_hw_map->clk_mux.ccf_handle));
	} else if (clk_hw_map->config_method == MMDVFS_CLK_CONFIG_PLL_RATE) {
		int clk_rate = clk_hw_map->step_pll_freq_map[clk_step];
		int pll_id = clk_hw_map->pll_id;

		if (pll_id == -1) {
			int ccf_ret = -1;

			if (clk_rate < 0) {
				MMDVFSMSG("invalid rate %d step:%d pll:%s\n",
				clk_rate, mmdvfs_step,
				clk_hw_map->clk_mux.ccf_name);
				return;
			}
			if (clk_hw_map->clk_mux.ccf_handle == NULL) {
				MMDVFSMSG("CCF handle can't be NULL\n");
				return;
			}

			ccf_ret = clk_set_rate(
				(struct clk *)clk_hw_map->clk_mux.ccf_handle,
				clk_rate);
			MMDVFSMSG("clk_set_rate: handle = (%lx), rate = %d\n",
			((unsigned long)clk_hw_map->clk_mux.ccf_handle),
			clk_rate);

			if (ccf_ret)
				MMDVFSMSG("Failed to set rate:%s->%d\n",
				clk_hw_map->clk_mux.ccf_name, clk_rate);
		} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
			int hopping_ret = -1;

			hopping_ret = mt_dfs_general_pll(pll_id, clk_rate);
			MMDVFSMSG("pll_hopping id=%d dds=0x%08x\n",
				pll_id, clk_rate);

			if (hopping_ret)
				MMDVFSMSG("Failed to hopping%d->0x%08x\n",
					pll_id, clk_rate);
#else
			MMDVFSMSG("hopping NOT SUPPORT: id = (%d)\n", pll_id);
#endif
		}
	}
}

static int mmdvfs_apply_clk_hw_configurtion_by_step(
	struct mmdvfs_adaptor *self, int mmdvfs_step_request, bool to_high)
{
	struct mmdvfs_hw_configurtion *hw_config_ptr = NULL;
	int i = 0;
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
	hw_config_ptr =
		&(self->step_profile_mappings + mmdvfs_step)->hw_config;

	/* Check if mmdvfs_hw_configurtion is found */
	if (hw_config_ptr == NULL)
		return -1;

	MMDVFSDEBUG(3, "CLK SWITCH: total = %d, step: (%d)\n",
		hw_config_ptr->total_clks, mmdvfs_step);

	/* Get each clk and setp it accord to config method   */
	if (to_high)
		for (i = 0; i < hw_config_ptr->total_clks; i++)
			mmdvfs_configure_clk_hw(
				self, hw_config_ptr, i, mmdvfs_step);
	else
		for (i = hw_config_ptr->total_clks - 1; i >= 0; i--)
			mmdvfs_configure_clk_hw(
				self, hw_config_ptr, i, mmdvfs_step);
	return 0;

}
static int mmdvfs_get_clients_clk_opp(struct mmdvfs_step_util *self,
	struct mmdvfs_adaptor *adaptor, int clients_mask, int clk_id)
{
	/* Get the opp determined only by the specified clients */
	int opp_idx = 0;
	int final_opp = -1;
	int final_clk_opp = -1;

	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
		int masked_concurrency =
		self->mmdvfs_concurrency_of_opps[opp_idx] & clients_mask;

		if (masked_concurrency != 0) {
			final_opp = opp_idx;
			break;
		}
	}

	/* if no request, return the lowerest step */
	if (final_opp == -1)
		final_opp = adaptor->step_num - 1;

	/* Retrieve the CLK opp setting associated the MMDVFS opp */
	if (clk_id >= 0	&& clk_id < adaptor->mmdvfs_clk_hw_maps_num) {
		if (final_opp >= 0 && final_opp	<= adaptor->step_num) {
			struct mmdvfs_step_profile *mmdvfs_step_to_profile =
			adaptor->step_profile_mappings + final_opp;
			final_clk_opp =
			mmdvfs_step_to_profile->hw_config.clk_steps[clk_id];
		}
	}
	return final_clk_opp;
}

static int mmdvfs_get_cam_sys_clk(
	struct mmdvfs_adaptor *self, int mmdvfs_step)
{
	struct mmdvfs_step_profile *profile =
		&self->step_profile_mappings[mmdvfs_step];
	return profile->hw_config.clk_steps[MMDVFS_CLK_MUX_TOP_CAM_SEL];
}

/* single_profile_dump_func: */
static void mmdvfs_single_profile_dump(struct mmdvfs_profile *profile)
{
	if (profile == NULL) {
		MMDVFSDEBUG(3, "profile_dump: NULL profile found\n");
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
		MMDVFSMSG("step_profile_mappings can't be NULL\n");
		return;
	}

	MMDVFSDEBUG(3, "MMDVFS DUMP (%d):\n", profile_mapping->mmdvfs_step);

	if (profile_mapping == NULL)
		MMDVFSDEBUG(3, "step_profile_mappings can't be NULL\n");
	MMDVFSDEBUG(3, "MMDVFS DUMP (%d):\n", profile_mapping->mmdvfs_step);
	for (i = 0; i < profile_mapping->total_profiles; i++) {
		struct mmdvfs_profile *profile =
		profile_mapping->profiles + i;
		mmdvfs_single_profile_dump(profile);
	}
}

static void mmdvfs_single_hw_configuration_dump(struct mmdvfs_adaptor *self,
	struct mmdvfs_hw_configurtion *hw_configuration)
{
	int i = 0;
	const int clk_soure_total = self->mmdvfs_clk_sources_num;
	struct mmdvfs_clk_source_desc *clk_sources = self->mmdvfs_clk_sources;
	struct mmdvfs_clk_hw_map *clk_hw_map = self->mmdvfs_clk_hw_maps;

	if (clk_hw_map == NULL) {
		MMDVFSMSG(
		"mmdvfs_clk_hw_maps can't be NULL\n");
		return;
	}

	if (hw_configuration == NULL) {
		MMDVFSMSG(
		"hw_configuration can't be NULL\n");
		return;
	}

	MMDVFSDEBUG(3, "Vcore tep: %d\n", hw_configuration->vcore_step);

	for (i = 0; i < hw_configuration->total_clks; i++) {
		char *ccf_clk_source_name = "NONE";
		char *ccf_clk_mux_name = "NONE";
		u32 clk_rate_mhz = 0;
		u32 clk_step = 0;
		u32 clk_id = 0;

		struct mmdvfs_clk_hw_map *map_item = clk_hw_map + i;

		clk_step = hw_configuration->clk_steps[i];

		if (map_item != NULL && map_item->clk_mux.ccf_name != NULL) {
			clk_id = map_item->step_clk_source_id_map[clk_step];
			ccf_clk_mux_name = map_item->clk_mux.ccf_name;
		}

		if (map_item->config_method == MMDVFS_CLK_CONFIG_BY_MUX
			&& clk_id < clk_soure_total) {
			ccf_clk_source_name = clk_sources[clk_id].ccf_name;
			clk_rate_mhz = clk_sources[clk_id].clk_rate_mhz;
		}

		MMDVFSDEBUG(3, "\t%s, %s, %dMhz\n",
		map_item->clk_mux.ccf_name,
		ccf_clk_source_name, clk_rate_mhz);
	}
}

static int mmdvfs_determine_step(struct mmdvfs_adaptor *self,
	int smi_scenario,
	struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_video_property *codec_setting)
{
	/* Find the matching scenario from OPP 0 to Max OPP */
	int opp_index =	0;
	int profile_index = 0;
	struct mmdvfs_step_profile *profile_mappings =
	self->step_profile_mappings;
	const int opp_max_num =	self->step_num;

	if (profile_mappings == NULL) {
		MMDVFSMSG(
		"step_profile_mappings can't be NULL\n");
		return MMDVFS_FINE_STEP_UNREQUEST;
	}

	for (opp_index = 0; opp_index < opp_max_num; opp_index++) {
		struct mmdvfs_step_profile *mapping_ptr =
		profile_mappings + opp_index;

		for (profile_index = 0;
			profile_index < mapping_ptr->total_profiles;
			profile_index++) {
			/* Check if the	scenario matches any profile */
			struct mmdvfs_profile *profile_prt =
				mapping_ptr->profiles + profile_index;
			if (smi_scenario == profile_prt->smi_scenario_id) {
				/* Check cam setting */
				if (!is_camera_profile_matched(
					cam_setting, &profile_prt->cam_limit))
					continue;
				if (!is_video_profile_matched(
					codec_setting,
					&profile_prt->video_limit))
					continue;
				/* Complete match, return the opp index */
				mmdvfs_single_profile_dump(profile_prt);
				return opp_index;
			}
		}
	}

	/* If there is no profile matched, return -1 (no dvfs request)*/
	return MMDVFS_FINE_STEP_UNREQUEST;
}

/* Show each setting of opp */
static void mmdvfs_hw_configuration_dump(struct mmdvfs_adaptor *self)
{
	int i =	0;
	struct mmdvfs_step_profile *mapping =
	self->step_profile_mappings;

	if (mapping == NULL) {
		MMDVFSMSG(
		"mmdvfs_clk_hw_maps can't be NULL");
		return;
	}

	MMDVFSDEBUG(3, "All OPP	configurtion dump\n");
	for (i = 0; i < self->step_num; i++) {
		struct mmdvfs_step_profile *mapping_item = mapping + i;

		MMDVFSDEBUG(3, "MMDVFS OPP %d:\n", i);
		if (mapping_item != NULL)
			self->single_hw_configuration_dump_func(
				self, &mapping_item->hw_config);
	}
}

static int is_camera_profile_matched(struct mmdvfs_cam_property *cam_setting,
	struct mmdvfs_cam_property *property)
{
	int is_match = 1;

	/* Null pointer check: */
	/* If the scenario doesn't has cam_setting, then there */
	/* is no need to check the cam property */
	if (!cam_setting || !property) {
		is_match = 1;
	} else {
		int feature_flag_mask_default = cam_setting->feature_flag
			& (~(MMDVFS_CAMERA_MODE_FLAG_DEFAULT));

		/* Check the minimum sensor resolution */
		if (cam_setting->sensor_size < property->sensor_size)
			is_match = 0;

		/* Check the minimum sensor resolution */
		if (cam_setting->fps < property->fps)
			is_match = 0;

		/* Check the minimum sensor resolution */
		if (cam_setting->preview_size < property->preview_size)
			is_match = 0;

		/* Check the if the feature match */
		/* Not match if there is no featue matching the	profile */
		/* 1 ==> don't change */
		/* 0 ==> set is_match to 0 */
		if (property->feature_flag != 0
		&& !(feature_flag_mask_default & property->feature_flag))
			is_match = 0;
	}

	return is_match;
}

static int is_video_profile_matched(
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
			/* Check the minimum sensor resolution */
			is_match = 0;
	}

	return is_match;
}

static void mmdvfs_step_util_init(struct mmdvfs_step_util *self)
{
	int idx	= 0;

	for (idx = 0; idx < self->total_scenario; idx++)
		self->mmdvfs_scenario_mmdvfs_opp[idx] =
		MMDVFS_FINE_STEP_UNREQUEST;

	for (idx = 0; idx < self->total_opps; idx++)
		self->mmdvfs_concurrency_of_opps[idx] = 0;
}

static inline void mmdvfs_adjust_scenario(s32 *mmdvfs_scen_opp_map,
	u32 changed_scenario, u32 new_scenario)
{
	if (mmdvfs_scen_opp_map[changed_scenario] >= 0) {
		mmdvfs_scen_opp_map[changed_scenario] = -1;
		MMDVFSDEBUG(5, "[adjust] new (%d) scenario (%d) to -1!\n",
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
static int mmdvfs_step_util_set_step(
	struct mmdvfs_step_util *self, s32 step, u32 scenario)
{
	int i = 0;
	int opp_idx = 0;
	int final_opp = -1;
	bool has_camera_scenario = false;

	/* check step range here */
	if (step < -1 || step >= self->total_opps)
		return MMDVFS_FINE_STEP_UNREQUEST;

	/* check invalid scenario */
	if (scenario >= self->total_scenario)
		return MMDVFS_FINE_STEP_UNREQUEST;

	self->mmdvfs_scenario_mmdvfs_opp[scenario] = step;

	if (self->wfd_vp_mix_step >= 0) {
		/* special configuration for mixed step */
		if (self->mmdvfs_scenario_mmdvfs_opp[SMI_BWC_SCEN_WFD] >= 0 &&
		self->mmdvfs_scenario_mmdvfs_opp[SMI_BWC_SCEN_VP] >= 0) {
			self->mmdvfs_scenario_mmdvfs_opp[MMDVFS_SCEN_VP_WFD] =
				self->wfd_vp_mix_step;
		} else {
			self->mmdvfs_scenario_mmdvfs_opp[MMDVFS_SCEN_VP_WFD] =
				MMDVFS_FINE_STEP_UNREQUEST;
		}
	}

	/* Reset the concurrency fileds before the calculation */
	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++)
		self->mmdvfs_concurrency_of_opps[opp_idx] = 0;

	for (i = 0; i < self->total_scenario; i++) {
		for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
			if (self->mmdvfs_scenario_mmdvfs_opp[i] == opp_idx)
				self->mmdvfs_concurrency_of_opps[opp_idx] |=
					1 << i;
		}
	}

	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
		if (self->mmdvfs_concurrency_of_opps[opp_idx] != 0) {
			final_opp = opp_idx;
			break;
		}
	}

	if (self->mmdvfs_scenario_mmdvfs_opp[MMDVFS_MGR] !=
		MMDVFS_FINE_STEP_UNREQUEST) {
		final_opp = self->mmdvfs_scenario_mmdvfs_opp[MMDVFS_MGR];
		MMDVFSMSG("[force] set step (%d)\n", final_opp);
	}

	for (opp_idx = 0; opp_idx < self->total_opps; opp_idx++) {
		if ((self->mmdvfs_concurrency_of_opps[opp_idx]
			& MMDVFS_CAMERA_SCEN_MASK) != 0) {
			has_camera_scenario = true;
			break;
		}
	}

	if (camera_bw_config && in_camera_scenario != has_camera_scenario) {
#if defined(SPECIAL_BW_CONFIG_MM)
		u32 bw_config = normal_bw_config;
		u32 old_bw_config, new_bw_config;

		if (has_camera_scenario)
			bw_config = camera_bw_config;
		MMDVFSDEBUG(3, "[DRAM setting] in camera? %d\n",
			has_camera_scenario);
		old_bw_config = BM_GetBW();
		BM_SetBW(bw_config);
		new_bw_config = BM_GetBW();
		MMDVFSDEBUG(3, "[DRAM] old:%#x, want:%#x, new:%#x\n",
			old_bw_config, bw_config, new_bw_config);
#else
		MMDVFSDEBUG(3, "[DRAM setting] not support\n");
#endif
	}
#ifdef DYNAMIC_DISP_HRT
	mmdvfs_change_disp_hrt(has_camera_scenario);
#endif
	in_camera_scenario = has_camera_scenario;
	return final_opp;
}

void mmdvfs_set_disp_hrt_cb(disp_hrt_change_cb change_cb)
{
	g_disp_hrt_change_cb = change_cb;
}

void mmdvfs_set_md_on(bool to_on)
{
	g_is_md_on = to_on;
#ifdef DYNAMIC_DISP_HRT
	mmdvfs_change_disp_hrt(in_camera_scenario);
#endif
}

#ifdef DYNAMIC_DISP_HRT
static void mmdvfs_change_disp_hrt(bool has_camera_scenario)
{
	if (cam_sensor_threshold && g_disp_hrt_change_cb) {
		struct mmdvfs_cam_property cam;
		u32 cam_sensor_setting;

		mmdvfs_internal_get_cam_setting(&cam);
		cam_sensor_setting = cam.sensor_size * cam.fps;
		if (has_camera_scenario &&
			cam_sensor_setting >= cam_sensor_threshold &&
			g_is_md_on) {
			MMDVFSMSG("decrease HRT with %d\n",
				disp_hrt_decrease_level1);
			g_disp_hrt_change_cb(disp_hrt_decrease_level1);
		} else {
			MMDVFSMSG("decrease HRT with 0\n");
			g_disp_hrt_change_cb(disp_hrt_decrease_default);
		}
	}
}
#endif


static int mmdvfs_get_opp_from_legacy_step(
	struct mmdvfs_step_util *self, int legacy_step)
{
	if (self->legacy_step_to_oop == NULL || legacy_step < 0 ||
		legacy_step >= self->legacy_step_to_oop_num)
		return -1;
	else
		return self->legacy_step_to_oop_num + legacy_step;
}

static int mmdvfs_get_legacy_mmclk_step_from_mmclk_opp(
	struct mmdvfs_step_util *self, int mmclk_step)
{
	int step_ret = -1;

	if (self->mmclk_oop_to_legacy_step == NULL || mmclk_step < 0
	|| mmclk_step >= self->mmclk_oop_to_legacy_step_num) {
		step_ret = -1;
	} else {
		int *step_ptr = self->mmclk_oop_to_legacy_step + mmclk_step;

		step_ret = -1;

		if (step_ptr != NULL)
			step_ret = *step_ptr;
	}
	return step_ret;
}

static const struct mmdvfs_vpu_steps_setting *get_vpu_setting_impl(
	struct mmdvfs_vpu_dvfs_configurator *self, int vpu_opp)
{
	if (vpu_opp < 0 || vpu_opp > self->nr_vpu_steps)
		return NULL;
	else
		return (const struct mmdvfs_vpu_steps_setting *)
			&(self->mmdvfs_vpu_steps_settings[vpu_opp]);
}

/* ISP DVFS Adaptor Impementation */
static s32 get_step_by_threshold(
	struct mmdvfs_thresholds_dvfs_handler *self, u32 class_id, u32 value)
{
		struct mmdvfs_threshold_setting *setting = NULL;
		int step_found = MMDVFS_FINE_STEP_UNREQUEST;
		int i = 0;

		if (class_id > self->mmdvfs_threshold_setting_num)
			return MMDVFS_FINE_STEP_UNREQUEST;

		if (value == 0)
			return MMDVFS_FINE_STEP_UNREQUEST;

		setting = &self->threshold_settings[class_id];

		for (i = 0;
			i < setting->thresholds_num;
			i++) {
			int *threshold = setting->thresholds + i;
			int *opp = setting->opps + i;

			if ((threshold) && (opp) && (value >= *threshold)) {
				MMDVFSDEBUG(5, "value=%d,threshold=%d\n",
					value, *threshold);
				step_found = *opp;
				break;
			}
		}
		if (step_found == MMDVFS_FINE_STEP_UNREQUEST)
			step_found = *(setting->opps +
				setting->thresholds_num-1);

		return step_found;
}

struct mmdvfs_vpu_dvfs_configurator *g_mmdvfs_vpu_adaptor;
struct mmdvfs_adaptor *g_mmdvfs_adaptor;
struct mmdvfs_adaptor *g_mmdvfs_non_force_adaptor;
struct mmdvfs_step_util *g_mmdvfs_step_util;
struct mmdvfs_step_util *g_non_force_step_util;
struct mmdvfs_thresholds_dvfs_handler *g_dvfs_handler;

#ifdef CONFIG_MTK_QOS_SUPPORT
static int mask_concur[MMDVFS_OPP_NUM_LIMITATION];
static void update_qos_scenario(void);
#endif

void mmdvfs_config_util_init(void)
{
	int mmdvfs_profile_id = mmdvfs_get_mmdvfs_profile();

	switch (mmdvfs_profile_id) {
	case MMDVFS_PROFILE_ZIO:
#if defined(SMI_ZIO)
		g_mmdvfs_adaptor = &mmdvfs_adaptor_obj_mt6739;
		g_mmdvfs_step_util = &mmdvfs_step_util_obj_mt6739;
		g_dvfs_handler = &dvfs_handler_mt6739;
#endif
		break;
	default:
		break;
	}

	g_mmdvfs_adaptor->profile_dump_func(g_mmdvfs_adaptor);
	MMDVFSMSG("g_mmdvfs_step_util init\n");

	if (g_mmdvfs_step_util)
		g_mmdvfs_step_util->init(g_mmdvfs_step_util);

	if (g_non_force_step_util)
		g_non_force_step_util->init(g_non_force_step_util);

#ifdef CONFIG_MTK_QOS_SUPPORT
	update_qos_scenario();
#endif
}

void mmdvfs_pm_qos_update_request(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 new_value)
{
	int step = MMDVFS_FINE_STEP_UNREQUEST;
	int mmdvfs_vote_id = -1;
	struct mmdvfs_threshold_setting *client_threshold_setting = NULL;

	if (!req)
		MMDVFSDEBUG(5, "single mode request\n");

	if (!g_dvfs_handler) {
		MMDVFSMSG("g_dvfs_handler is NULL\n");
		return;
	}

	client_threshold_setting =
		g_dvfs_handler->threshold_settings
		+ mmdvfs_pm_qos_class;

	if (!client_threshold_setting) {
		MMDVFSMSG("client_threshold_setting is NULL\n");
		return;
	}

	mmdvfs_vote_id = client_threshold_setting->mmdvfs_client_id;
	step = g_dvfs_handler->get_step(
		g_dvfs_handler,
		mmdvfs_pm_qos_class, new_value);

	mmdvfs_set_fine_step(mmdvfs_vote_id, step);
}

void mmdvfs_pm_qos_remove_request(struct mmdvfs_pm_qos_request *req)
{
	if (!req) {
		MMDVFSDEBUG(5, "can't be NULL for PMQoS\n");
		return;
	}
	mmdvfs_pm_qos_update_request(req, req->pm_qos_class, 0);
}

void mmdvfs_pm_qos_add_request(
	struct mmdvfs_pm_qos_request *req, u32 mmdvfs_pm_qos_class, u32 value)
{
	if (!req) {
		MMDVFSDEBUG(5, "mmdvfs_pm_qos_request can't be NULL(add)\n");
		return;
	}

	mmdvfs_pm_qos_update_request(req, mmdvfs_pm_qos_class, value);
}

static inline struct mmdvfs_threshold_setting *get_threshold_setting(
	u32 class_id)
{
	if (class_id >
	g_dvfs_handler->mmdvfs_threshold_setting_num)
		return NULL;

	return &g_dvfs_handler->threshold_settings[class_id];
}

u32 mmdvfs_qos_get_thres_count(
	struct mmdvfs_pm_qos_request *req, u32 mmdvfs_pm_qos_class)
{
	struct mmdvfs_threshold_setting *setting_ptr =
		get_threshold_setting(mmdvfs_pm_qos_class);

	if (setting_ptr)
		return setting_ptr->thresholds_num;

	return 0;
}

u32 mmdvfs_qos_get_thres_value(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class, u32 thres_idx)
{
	struct mmdvfs_threshold_setting *setting_ptr =
		get_threshold_setting(mmdvfs_pm_qos_class);

	if (setting_ptr) {
		if (thres_idx > setting_ptr->thresholds_num)
			return 0;
		return setting_ptr->thresholds[thres_idx];
	}
	return 0;
}

u32 mmdvfs_qos_get_cur_thres(struct mmdvfs_pm_qos_request *req,
	u32 mmdvfs_pm_qos_class)
{
	struct mmdvfs_clk_hw_map *hw_map_ptr = NULL;
	u32 clk_step = 0, config_method = 0, clk_rate_mhz = 0;
	s32 current_step = mmdvfs_get_current_fine_step();

	if (current_step < 0 ||
		mmdvfs_pm_qos_class != MMDVFS_PM_QOS_SUB_SYS_CAMERA)
		return 0;

	clk_step = g_mmdvfs_adaptor->get_cam_sys_clk(
		g_mmdvfs_adaptor, current_step);
	hw_map_ptr =
	&g_mmdvfs_adaptor->mmdvfs_clk_hw_maps[MMDVFS_CLK_MUX_TOP_CAM_SEL];
	config_method = hw_map_ptr->config_method;
	if (config_method == MMDVFS_CLK_CONFIG_PLL_RATE)
		clk_rate_mhz =
		hw_map_ptr->step_pll_freq_map[clk_step] / 1000000;
	else if (config_method == MMDVFS_CLK_CONFIG_BY_MUX) {
		u32 clk_id = hw_map_ptr->step_clk_source_id_map[clk_step];

		clk_rate_mhz =
		g_mmdvfs_adaptor->mmdvfs_clk_sources[clk_id].clk_rate_mhz;
	}
	return clk_rate_mhz;
}

#ifdef CONFIG_MTK_QOS_SUPPORT
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
	int *concur = step_util->mmdvfs_concurrency_of_opps;

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
				step_util->mmdvfs_concurrency_of_opps[i]);
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
		MMDVFSMSG("scenario %d not in %d~%d\n",
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
