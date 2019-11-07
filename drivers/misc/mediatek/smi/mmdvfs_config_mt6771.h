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

#ifndef __MMDVFS_CONFIG_MT6771_H__
#define __MMDVFS_CONFIG_MT6771_H__

#include "mmdvfs_config_util.h"
#include "mtk_vcorefs_manager.h"

/* Part I MMSVFS HW Configuration (OPP)*/
/* Define the number of mmdvfs, vcore and mm clks opps */

#define MT6771_MMDVFS_OPP_MAX 4
#define MT6771_MMDVFS_CLK_OPP_MAX 4
#define MT6771_MMDVFS_VCORE_OPP_MAX 4

/* CLK source configuration */

/* CLK source IDs */
/* Define the internal index of each CLK source*/
#define MT6771_MMDVFS_CLK_SYSPLL_D2_CK 0
#define MT6771_MMDVFS_CLK_MMPLL_D6_CK 1
#define MT6771_MMDVFS_CLK_MMPLL_D7_CK 2
#define MT6771_MMDVFS_CLK_SYSPLL_D3_CK 3
#define MT6771_MMDVFS_CLK_UNIVPLL_D2_D2_CK 4
#define MT6771_CLK_SOURCE_NUM 5

/* CLK Source definiation */
/* Define the clk source description */
struct mmdvfs_clk_source_desc mt6771_clk_sources[MT6771_CLK_SOURCE_NUM] = {
		{NULL, "mmdvfs_clk_syspll_d2_ck", 546},
		{NULL, "mmdvfs_clk_mmpll_d6_ck", 525},
		{NULL, "mmdvfs_clk_mmpll_d7_ck", 450},
		{NULL, "mmdvfs_clk_syspll_d3_ck", 364},
		{NULL, "mmdvfs_clk_univpll_d2_d2_ck", 312},
};

/*
 * B. CLK Change adaption configurtion
 * B.1 Define the clk change method and data of each MM CLK step
 * Field decscription:
 * 1. config_method:
 *     a. MMDVFS_CLK_CONFIG_BY_MUX
 *     b. MMDVFS_CLK_CONFIG_PLL
 *     c. MMDVFS_CLK_CONFIG_NONE
 * 2. pll_id: PLL ID, please set -1 if PLL hopping is not used
 * 3. clk mux desc {hanlde, name}, plz set -1
 *       and it will be initialized by driver automaticlly
 * 4. total step: the number of the steps supported by this sub sys
 * 5. hopping dss of each steps: please set -1 if it is not used
 * 6. clk sources id of each steps: please set -1 if it is not used
 */
struct mmdvfs_clk_hw_map mt6771_mmdvfs_clk_hw_map[MMDVFS_CLK_MUX_NUM] = {
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_SMI0_2X_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX,
			{ NULL, "MMDVFS_CLK_MUX_TOP_MM_SEL"}, -1, 2,
			{-1, -1},
			{MT6771_MMDVFS_CLK_MMPLL_D7_CK,
			MT6771_MMDVFS_CLK_UNIVPLL_D2_D2_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX,
			{ NULL, "MMDVFS_CLK_MUX_TOP_CAM_SEL"}, -1, 2,
			{-1, -1},
			{MT6771_MMDVFS_CLK_SYSPLL_D2_CK,
			MT6771_MMDVFS_CLK_SYSPLL_D3_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX,
			{ NULL, "MMDVFS_CLK_MUX_TOP_IMG_SEL"}, -1, 2,
			{-1, -1},
			{MT6771_MMDVFS_CLK_MMPLL_D6_CK,
			MT6771_MMDVFS_CLK_SYSPLL_D3_CK}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_VENC_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_VDEC_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_MJC_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_VPU_IF_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_VPU_IF_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_VPU_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		}
};

struct mmdvfs_profile_mask qos_apply_profiles[] = {
#ifdef CONFIG_MTK_QOS_SUPPORT
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, MMDVFS_FINE_STEP_OPP0 },
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, MMDVFS_FINE_STEP_OPP1 },
	{"4K VP", SMI_BWC_SCEN_VP_HIGH_RESOLUTION, MMDVFS_FINE_STEP_OPP0 },
	{"4K VP", SMI_BWC_SCEN_VP_HIGH_RESOLUTION, MMDVFS_FINE_STEP_OPP1 },
	{"VPU", MMDVFS_SCEN_VPU_KERNEL, MMDVFS_FINE_STEP_OPP0 },
	{"ICFP", SMI_BWC_SCEN_ICFP, MMDVFS_FINE_STEP_OPP0},
	{"ICFP", SMI_BWC_SCEN_ICFP, MMDVFS_FINE_STEP_OPP1},
	{"DEBUG", 0, MMDVFS_FINE_STEP_UNREQUEST },	/* debug entry */
#else
	{"DEBUG", QOS_ALL_SCENARIO, 0 },	/* debug entry */
#endif
};

/* Part II MMDVFS Scenario's Step Confuguration */

#define MT6771_MMDVFS_SENSOR_MIN (25000000)
/* A.1 [LP4 2-ch] Scenarios of each MM DVFS Step (force kicker) */
/* OPP 0 scenarios */
#define MT6771_MMDVFS_OPP0_NUM 12
struct mmdvfs_profile mt6771_mmdvfs_opp0_profiles[MT6771_MMDVFS_OPP0_NUM] = {
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, {0, 0, 0}, {0, 0, 0 } },
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Capture (ZSD)", SMI_BWC_SCEN_CAM_CP,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Preview (ZSD)", SMI_BWC_SCEN_CAM_PV,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Camera Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Stereo Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS 4K Feature Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0},
		{0, 0, 0 } },
	{"4K VR/ VSS (VENC)", SMI_BWC_SCEN_VENC, {0, 0, 0}, {4096, 1716, 0} },
	{"High resolution video playback", SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
		{0, 0, 0}, {0, 0, 0 } },
};

/* OPP 1 scenarios */
#define MT6771_MMDVFS_OPP1_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp1_profiles[MT6771_MMDVFS_OPP1_NUM] = {
};

/* OPP 2 scenarios */
#define MT6771_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp2_profiles[MT6771_MMDVFS_OPP2_NUM] = {
};

/* OPP 3 scenarios */
#define MT6771_MMDVFS_OPP3_NUM 11
struct mmdvfs_profile mt6771_mmdvfs_opp3_profiles[MT6771_MMDVFS_OPP3_NUM] = {
	{"Dual zoom preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS, {0, 0, 0}, {0, 0, 0 } },
	{"VENC", SMI_BWC_SCEN_VENC, {0, 0, 0}, {0, 0, 0} },
	{"MHL", MMDVFS_SCEN_MHL, {0, 0, 0}, {0, 0, 0 } },
	{"WFD", SMI_BWC_SCEN_WFD, {0, 0, 0}, {0, 0, 0 } },
	{"High frame rate video playback", SMI_BWC_SCEN_VP_HIGH_FPS,
		{0, 0, 0}, {0, 0, 0 } },
};

/* A.1 [LP4 2-ch (3600)] Scenarios of each MM DVFS Step (force kicker) */
/* OPP 0 scenarios */
#define MT6771_3600_MMDVFS_OPP0_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp0_profiles_3600[
				MT6771_3600_MMDVFS_OPP0_NUM] = {
};

/* OPP 1 scenarios */
#define MT6771_3600_MMDVFS_OPP1_NUM 12
struct mmdvfs_profile mt6771_mmdvfs_opp1_profiles_3600[
				MT6771_3600_MMDVFS_OPP1_NUM] = {
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, {0, 0, 0}, {0, 0, 0 } },
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Capture (ZSD)", SMI_BWC_SCEN_CAM_CP,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Preview (ZSD)", SMI_BWC_SCEN_CAM_PV,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Camera Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Stereo Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS 4K Feature Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0},
		{0, 0, 0 } },
	{"4K VR/ VSS (VENC)", SMI_BWC_SCEN_VENC, {0, 0, 0}, {4096, 1716, 0} },
	{"High resolution video playback", SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
		{0, 0, 0}, {0, 0, 0 } },
};

/* OPP 2 scenarios */
#define MT6771_3600_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp2_profiles_3600[
				MT6771_3600_MMDVFS_OPP2_NUM] = {
};

/* OPP 3 scenarios */
#define MT6771_3600_MMDVFS_OPP3_NUM 11
struct mmdvfs_profile mt6771_mmdvfs_opp3_profiles_3600[
					MT6771_3600_MMDVFS_OPP3_NUM] = {
	{"Dual zoom preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS, {0, 0, 0}, {0, 0, 0 } },
	{"VENC", SMI_BWC_SCEN_VENC, {0, 0, 0}, {0, 0, 0} },
	{"MHL", MMDVFS_SCEN_MHL, {0, 0, 0}, {0, 0, 0 } },
	{"WFD", SMI_BWC_SCEN_WFD, {0, 0, 0}, {0, 0, 0 } },
	{"High frame rate video playback", SMI_BWC_SCEN_VP_HIGH_FPS,
		{0, 0, 0}, {0, 0, 0 } },
};

/* A.2 [LP3] Scenarios of each MM DVFS Step (force kicker) */
/* OOP 0 scenarios */
#define MT6771_LP3_MMDVFS_OPP0_NUM 10
struct mmdvfs_profile mt6771_mmdvfs_opp0_profiles_lp3[
					MT6771_LP3_MMDVFS_OPP0_NUM] = {
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, {0, 0, 0}, {0, 0, 0 } },
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Capture (ZSD)", SMI_BWC_SCEN_CAM_CP,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Stereo Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS 4K Feature Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0},
		{0, 0, 0 } },
	{"4K VR/ VSS (VENC)", SMI_BWC_SCEN_VENC, {0, 0, 0}, {4096, 1716, 0} },
	{"High resolution video playback", SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
		{0, 0, 0}, {0, 0, 0 } },
};

/* OPP 1 scenarios */
#define MT6771_LP3_MMDVFS_OPP1_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp1_profiles_lp3[
				MT6771_LP3_MMDVFS_OPP1_NUM] = {
};

/* OPP 2 scenarios */
#define MT6771_LP3_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6771_mmdvfs_opp2_profiles_lp3[
				MT6771_LP3_MMDVFS_OPP2_NUM] = {
};

/* OPP 3 scenarios */
#define MT6771_LP3_MMDVFS_OPP3_NUM 13
struct mmdvfs_profile mt6771_mmdvfs_opp3_profiles_lp3[
					MT6771_LP3_MMDVFS_OPP3_NUM] = {
	{"Full Sensor Preview (ZSD)", SMI_BWC_SCEN_CAM_PV,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Camera Recording", SMI_BWC_SCEN_VR,
		{MT6771_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Dual zoom preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS, {0, 0, 0}, {0, 0, 0 } },
	{"VENC", SMI_BWC_SCEN_VENC, {0, 0, 0}, {0, 0, 0} },
	{"MHL", MMDVFS_SCEN_MHL, {0, 0, 0}, {0, 0, 0 } },
	{"WFD", SMI_BWC_SCEN_WFD, {0, 0, 0}, {0, 0, 0 } },
	{"High frame rate video playback", SMI_BWC_SCEN_VP_HIGH_FPS,
		{0, 0, 0}, {0, 0, 0 } },
};

/* Defined the smi scenarios whose DVFS is controlled by low-level driver */
/* directly, not by BWC scenario change event */
#define MT6771_MMDVFS_SMI_USER_CONTROL_SCEN_MASK (1 << SMI_BWC_SCEN_VP)

struct mmdvfs_step_to_qos_step legacy_to_qos_step[MT6771_MMDVFS_OPP_MAX] = {
	{0, 0},
	{1, 1},
	{2, 1},
	{3, 1},
};

/* Part III Scenario and MMSVFS HW configuration mapping */
/* 1. For a single mmdvfs step's profiles and hardware configuration */
/* LP4 2-ch */
struct mmdvfs_step_profile mt6771_step_profile[MT6771_MMDVFS_OPP_MAX] = {
		{0, mt6771_mmdvfs_opp0_profiles, MT6771_MMDVFS_OPP0_NUM,
		{OPP_0,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{1, mt6771_mmdvfs_opp1_profiles, MT6771_MMDVFS_OPP1_NUM,
		{OPP_1,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{2, mt6771_mmdvfs_opp2_profiles, MT6771_MMDVFS_OPP2_NUM,
		{OPP_2,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{3, mt6771_mmdvfs_opp3_profiles, MT6771_MMDVFS_OPP3_NUM,
		{OPP_3,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
};

/* LP4 2-ch (3600) */
struct mmdvfs_step_profile mt6771_step_profile_3600[MT6771_MMDVFS_OPP_MAX] = {
		{0, mt6771_mmdvfs_opp0_profiles_3600,
		MT6771_3600_MMDVFS_OPP0_NUM,
		{OPP_0,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{1, mt6771_mmdvfs_opp1_profiles_3600,
		MT6771_3600_MMDVFS_OPP1_NUM,
		{OPP_1,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{2, mt6771_mmdvfs_opp2_profiles_3600,
		MT6771_3600_MMDVFS_OPP2_NUM,
		{OPP_2,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{3, mt6771_mmdvfs_opp3_profiles_3600,
		MT6771_3600_MMDVFS_OPP3_NUM,
		{OPP_3,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
};

/* LP3 */
struct mmdvfs_step_profile mt6771_step_profile_lp3[MT6771_MMDVFS_OPP_MAX] = {
		{0, mt6771_mmdvfs_opp0_profiles_lp3,
		MT6771_LP3_MMDVFS_OPP0_NUM,
		{OPP_0,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{1, mt6771_mmdvfs_opp1_profiles_lp3,
		MT6771_LP3_MMDVFS_OPP1_NUM,
		{OPP_1,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{2, mt6771_mmdvfs_opp2_profiles_lp3,
		MT6771_LP3_MMDVFS_OPP2_NUM,
		{OPP_2,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{3, mt6771_mmdvfs_opp3_profiles_lp3,
		MT6771_LP3_MMDVFS_OPP3_NUM,
		{OPP_3,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
};

/* Part III Scenario and MMSVFS HW configuration mapping */

#define MT6771_MMDVFS_VOLTAGE_LOW_OPP	2
#define MT6771_MMDVFS_VOLTAGE_HIGH_OPP	1
#define MT6771_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP	-1
#define MT6771_MMDVFS_VOLTAGE_LOW_LOW_OPP 3

int mt6771_mmdvfs_legacy_step_to_opp[MMDVFS_VOLTAGE_COUNT] = {
	MT6771_MMDVFS_VOLTAGE_LOW_OPP,
	MT6771_MMDVFS_VOLTAGE_HIGH_OPP,
	MT6771_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP,
	MT6771_MMDVFS_VOLTAGE_LOW_LOW_OPP
};

#define MT6771_MMCLK_OPP0_LEGACY_STEP	MMSYS_CLK_HIGH
#define MT6771_MMCLK_OPP1_LEGACY_STEP	MMSYS_CLK_LOW
/* MMCLK_OPP3 and OPP2 is not used in this configuration */
#define MT6771_MMCLK_OPP2_LEGACY_STEP	MMSYS_CLK_LOW
#define MT6771_MMCLK_OPP3_LEGACY_STEP	MMSYS_CLK_LOW

int mt6771_mmdvfs_mmclk_opp_to_legacy_mmclk_step[MT6771_MMDVFS_OPP_MAX] = {
	MT6771_MMCLK_OPP0_LEGACY_STEP, MT6771_MMCLK_OPP1_LEGACY_STEP,
	MT6771_MMCLK_OPP2_LEGACY_STEP, MT6771_MMCLK_OPP3_LEGACY_STEP
};


/* Part IV VPU association */
/* There is no more need VPU DVFS MMDVFS */

/* Part V ISP DVFS configuration */
#define MMDVFS_ISP_THRESHOLD_NUM 2
int mt6771_mmdvs_isp_threshold_setting[MMDVFS_ISP_THRESHOLD_NUM] = {546, 364};
int mt6771_mmdvs_isp_threshold_opp[MMDVFS_ISP_THRESHOLD_NUM] = {
			MMDVFS_FINE_STEP_OPP0, MMDVFS_FINE_STEP_OPP3};
int mt6771_mmdvs_isp_threshold_opp_3600[MMDVFS_ISP_THRESHOLD_NUM] = {
			MMDVFS_FINE_STEP_OPP1, MMDVFS_FINE_STEP_OPP3};

struct mmdvfs_threshold_setting mt6771_mmdvfs_threshold_settings[
			MMDVFS_PMQOS_NUM] = {
	{ MMDVFS_PM_QOS_SUB_SYS_CAMERA, mt6771_mmdvs_isp_threshold_setting,
	mt6771_mmdvs_isp_threshold_opp, MMDVFS_ISP_THRESHOLD_NUM,
	MMDVFS_PMQOS_ISP},
};

struct mmdvfs_threshold_setting mt6771_mmdvfs_threshold_settings_3600[
			MMDVFS_PMQOS_NUM] = {
	{ MMDVFS_PM_QOS_SUB_SYS_CAMERA, mt6771_mmdvs_isp_threshold_setting,
	mt6771_mmdvs_isp_threshold_opp_3600, MMDVFS_ISP_THRESHOLD_NUM,
	MMDVFS_PMQOS_ISP},
};

#endif /* __MMDVFS_CONFIG_MT6771_H__ */

