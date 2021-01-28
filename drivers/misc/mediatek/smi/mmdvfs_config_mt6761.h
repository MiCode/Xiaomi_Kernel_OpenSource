/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MMDVFS_CONFIG_MT6761_H__
#define __MMDVFS_CONFIG_MT6761_H__

#include "mmdvfs_config_util.h"

/* Part I MMSVFS HW Configuration (OPP)*/
/* Define the number of mmdvfs, vcore and mm clks opps */

/* Max total MMDVFS opps of the profile support */
#define MT6761_MMDVFS_OPP_MAX 3

struct mmdvfs_profile_mask qos_apply_profiles[] = {
/* #ifdef MMDVFS_QOS_SUPPORT */
	/* ISP for opp0 */
	{"ISP",
		SMI_BWC_SCEN_CAM_PV,
		MMDVFS_FINE_STEP_OPP0},
	{"ISP",
		SMI_BWC_SCEN_CAM_CP,
		MMDVFS_FINE_STEP_OPP0},
	{"ISP",
		SMI_BWC_SCEN_VR,
		MMDVFS_FINE_STEP_OPP0},
	/* ISP for opp1 */
	{"ISP",
		SMI_BWC_SCEN_CAM_PV,
		MMDVFS_FINE_STEP_OPP1},
	{"ISP",
		SMI_BWC_SCEN_CAM_CP,
		MMDVFS_FINE_STEP_OPP1},
	{"ISP",
		SMI_BWC_SCEN_VR,
		MMDVFS_FINE_STEP_OPP1},
	/* ISP for opp2 */
	{"ISP",
		SMI_BWC_SCEN_CAM_PV,
		MMDVFS_FINE_STEP_OPP2},
	{"ISP",
		SMI_BWC_SCEN_CAM_CP,
		MMDVFS_FINE_STEP_OPP2},
	{"ISP",
		SMI_BWC_SCEN_VR,
		MMDVFS_FINE_STEP_OPP2},
	/* ICFP for opp0 */
	{"ICFP",
		SMI_BWC_SCEN_ICFP,
		MMDVFS_FINE_STEP_OPP0},
	/* debug entry */
	{"DEBUG",
		0,
		MMDVFS_FINE_STEP_UNREQUEST },
};

/* Part II MMDVFS Scenario's Step Confuguration */

#define MT6761_MMDVFS_SENSOR_MIN (13000000)
#define MT6761_MMDVFS_SENSOR_MID (16000000)
/* A.1 [LP4 2-ch] Scenarios of each MM DVFS Step (force kicker) */
/* OPP 0 scenarios */
#define MT6761_MMDVFS_OPP0_NUM 1
struct mmdvfs_profile mt6761_mmdvfs_opp0_profiles[MT6761_MMDVFS_OPP0_NUM] = {
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
/*
 *	{"Camera Preview", SMI_BWC_SCEN_CAM_PV,
 *		{MT6761_MMDVFS_SENSOR_MID, 0, 0}, {0, 0, 0 } },
 *	{"Camera Capture", SMI_BWC_SCEN_CAM_CP,
 *		{MT6761_MMDVFS_SENSOR_MID, 0, 0}, {0, 0, 0 } },
 *	{"Video Recording", SMI_BWC_SCEN_VR,
 *		{MT6761_MMDVFS_SENSOR_MID, 0, 0}, {0, 0, 0 } },
 */
};

/* OPP 1 scenarios */
#define MT6761_MMDVFS_OPP1_NUM 0
struct mmdvfs_profile mt6761_mmdvfs_opp1_profiles[MT6761_MMDVFS_OPP1_NUM] = {
/*
 *	{"Camera Preview", SMI_BWC_SCEN_CAM_PV,
 *		{MT6761_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
 *	{"Camera Capture", SMI_BWC_SCEN_CAM_CP,
 *		{MT6761_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
 *	{"Video Recording", SMI_BWC_SCEN_VR,
 *		{MT6761_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
 */
};

/* OPP 2 scenarios */
#define MT6761_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6761_mmdvfs_opp2_profiles[MT6761_MMDVFS_OPP2_NUM] = {
/*
 *	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
 *	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
 *	{"Video Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
 */
};

struct mmdvfs_step_to_qos_step legacy_to_qos_step[MT6761_MMDVFS_OPP_MAX] = {
	{0, 0},
	{1, 1},
	{2, 2},
};

struct mmdvfs_step_profile mt6761_step_profile[MT6761_MMDVFS_OPP_MAX] = {
	{0, mt6761_mmdvfs_opp0_profiles, MT6761_MMDVFS_OPP0_NUM, {0} },
	{1, mt6761_mmdvfs_opp1_profiles, MT6761_MMDVFS_OPP1_NUM, {0} },
	{2, mt6761_mmdvfs_opp2_profiles, MT6761_MMDVFS_OPP2_NUM, {0} },
};
#endif /* __MMDVFS_CONFIG_MT6761_H__ */
