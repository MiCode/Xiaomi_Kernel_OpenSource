/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MMDVFS_CONFIG_MT6739_H__
#define __MMDVFS_CONFIG_MT6739_H__

#include "mmdvfs_config_util.h"
#include "mtk_vcorefs_manager.h"

/* Part I MMSVFS HW Configuration (OPP)*/
/* Define the number of mmdvfs, vcore and mm clks opps */

#define MT6739_MMDVFS_OPP_MAX 4
#define MT6739_MMDVFS_CLK_OPP_MAX 4
#define MT6739_MMDVFS_VCORE_OPP_MAX 4

/* CLK source configuration */

/* CLK source IDs */
/* Define the internal index of each CLK source*/
#define MT6739_MMDVFS_CLK_TOP_SYSPLL2_D2 0
#define MT6739_MMDVFS_CLK_TOP_VENCPLL_CK 1
#define MT6739_CLK_SOURCE_NUM 2

/* CLK Source definiation */
/* Define the clk source description */
struct mmdvfs_clk_source_desc mt6739_clk_sources[MT6739_CLK_SOURCE_NUM] = {
		{NULL, "mmdvfs_clk_top_syspll2_d2", 182},
		{NULL, "mmdvfs_clk_top_vencpll_ck", 300},
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
 * 3. clk mux desc {handle, name}, plz set -1
 *       and it will be initialized by driver automaticlly
 * 4. total step: the number of the steps supported by this sub sys
 * 5. hopping dss of each steps: please set -1 if it is not used
 * 6. clk sources id of each steps: please set -1 if it is not used
 */
struct mmdvfs_clk_hw_map mt6739_mmdvfs_clk_hw_map[MMDVFS_CLK_MUX_NUM] = {
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_SMI0_2X_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX,
			{ NULL, "MMDVFS_CLK_TOP_MMPLL_CK"}, -1, 2,
			{-1, -1},
			{MT6739_MMDVFS_CLK_TOP_VENCPLL_CK,
			MT6739_MMDVFS_CLK_TOP_SYSPLL2_D2}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_CAM_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,
			{ NULL, "MMDVFS_CLK_MUX_TOP_IMG_SEL"}, -1, 2,
			{-1, -1},
			{-1, -1}
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

/* Part II MMDVFS Scenario's Step Confuguration */

#define MT6739_MMDVFS_SENSOR_MIN (7900000)
/* A.1 [LP4 2-ch] Scenarios of each MM DVFS Step (force kicker) */
/* OPP 0 scenarios */
#define MT6739_MMDVFS_OPP0_NUM 11
struct mmdvfs_profile mt6739_mmdvfs_opp0_profiles[MT6739_MMDVFS_OPP0_NUM] = {
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Preview (ZSD)", SMI_BWC_SCEN_CAM_PV,
		{MT6739_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Capture (ZSD)", SMI_BWC_SCEN_CAM_CP,
		{MT6739_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Camera Recording", SMI_BWC_SCEN_VR,
		{MT6739_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS,
		{MT6739_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"PIP Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_PIP, 0}, {0, 0, 0 } },
	{"PIP Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_PIP, 0}, {0, 0, 0 } },
	{"PIP Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_PIP, 0}, {0, 0, 0 } },
	{"FHD VR/ VSS (VENC)", SMI_BWC_SCEN_VENC,
		{0, 0, 0}, {1920, 1080, 0} },
	{"High resolution video playback", SMI_BWC_SCEN_VP_HIGH_RESOLUTION,
		{0, 0, 0}, {0, 0, 0 } },
	{"WFD", SMI_BWC_SCEN_WFD, {0, 0, 0}, {0, 0, 0 } },
};

/* OPP 1 scenarios */
#define MT6739_MMDVFS_OPP1_NUM 0
struct mmdvfs_profile mt6739_mmdvfs_opp1_profiles[MT6739_MMDVFS_OPP1_NUM] = {
};

/* OPP 2 scenarios */
#define MT6739_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6739_mmdvfs_opp2_profiles[MT6739_MMDVFS_OPP2_NUM] = {
};

/* OPP 3 scenarios */
#define MT6739_MMDVFS_OPP3_NUM 19
struct mmdvfs_profile mt6739_mmdvfs_opp3_profiles[MT6739_MMDVFS_OPP3_NUM] = {
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, {0, 0, 0}, {0, 0, 0 } },
	{"EIS Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS, {0, 0, 0}, {0, 0, 0 } },
	{"VENC", SMI_BWC_SCEN_VENC, {0, 0, 0}, {0, 0, 0} },
	{"MHL", MMDVFS_SCEN_MHL, {0, 0, 0}, {0, 0, 0 } },
	{"High frame rate video playback", SMI_BWC_SCEN_VP_HIGH_FPS,
		{0, 0, 0}, {0, 0, 0 } },
	{"vFB Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
	{"vFB Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
	{"vFB Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
	{"Stereo Feature Preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Capture", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Recording", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Dual zoom preview", SMI_BWC_SCEN_CAM_PV,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS 4K Feature Recording", SMI_BWC_SCEN_VR,
		{MT6739_MMDVFS_SENSOR_MIN,
		MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0},
		{0, 0, 0 } },
};

/* Defined the smi scenarios whose DVFS is controlled by low-level driver */
/* directly, not by BWC scenario change event */
#define MT6739_MMDVFS_SMI_USER_CONTROL_SCEN_MASK (1 << SMI_BWC_SCEN_VP)

/* Part III Scenario and MMSVFS HW configuration mapping */
/* 1. For a single mmdvfs step's profiles and hardware configuration */
struct mmdvfs_step_profile mt6739_step_profile[MT6739_MMDVFS_OPP_MAX] = {
		{0, mt6739_mmdvfs_opp0_profiles, MT6739_MMDVFS_OPP0_NUM,
		{OPP_0,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{1, mt6739_mmdvfs_opp1_profiles, MT6739_MMDVFS_OPP1_NUM,
		{OPP_1,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{2, mt6739_mmdvfs_opp2_profiles, MT6739_MMDVFS_OPP2_NUM,
		{OPP_2,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{3, mt6739_mmdvfs_opp3_profiles, MT6739_MMDVFS_OPP3_NUM,
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

#define MT6739_MMDVFS_VOLTAGE_LOW_OPP	3
#define MT6739_MMDVFS_VOLTAGE_HIGH_OPP	0
#define MT6739_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP	-1
#define MT6739_MMDVFS_VOLTAGE_LOW_LOW_OPP 3

int mt6739_mmdvfs_legacy_step_to_opp[MMDVFS_VOLTAGE_COUNT] = {
	MT6739_MMDVFS_VOLTAGE_LOW_OPP,
	MT6739_MMDVFS_VOLTAGE_HIGH_OPP,
	MT6739_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP,
	MT6739_MMDVFS_VOLTAGE_LOW_LOW_OPP
};

#define MT6739_MMCLK_OPP0_LEGACY_STEP	MMSYS_CLK_HIGH
#define MT6739_MMCLK_OPP1_LEGACY_STEP	MMSYS_CLK_LOW
/* MMCLK_OPP3 and OPP2 is not used in this configuration */
#define MT6739_MMCLK_OPP2_LEGACY_STEP	MMSYS_CLK_LOW
#define MT6739_MMCLK_OPP3_LEGACY_STEP	MMSYS_CLK_LOW

int mt6739_mmdvfs_mmclk_opp_to_legacy_mmclk_step[MT6739_MMDVFS_OPP_MAX] = {
	MT6739_MMCLK_OPP0_LEGACY_STEP, MT6739_MMCLK_OPP1_LEGACY_STEP,
	MT6739_MMCLK_OPP2_LEGACY_STEP, MT6739_MMCLK_OPP3_LEGACY_STEP
};


/* Part IV VPU association */
/* There is no VPU DVFS in MT6739 */

/* Part V ISP DVFS configuration */
#define MMDVFS_ISP_THRESHOLD_NUM 2
int mt6739_mmdvs_isp_threshold_setting[MMDVFS_ISP_THRESHOLD_NUM] = {300, 182};
int mt6739_mmdvs_isp_threshold_opp[MMDVFS_ISP_THRESHOLD_NUM] = {
	MMDVFS_FINE_STEP_OPP0, MMDVFS_FINE_STEP_OPP3};

struct mmdvfs_threshold_setting mt6739_mmdvfs_threshold[MMDVFS_PMQOS_NUM] = {
	{ MMDVFS_PM_QOS_SUB_SYS_CAMERA, mt6739_mmdvs_isp_threshold_setting,
	mt6739_mmdvs_isp_threshold_opp,
	MMDVFS_ISP_THRESHOLD_NUM, MMDVFS_PMQOS_ISP},
};


#endif /* __MMDVFS_CONFIG_MT6739_H__ */
