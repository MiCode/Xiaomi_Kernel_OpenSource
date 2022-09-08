/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/******************************************************************************
 * cam_common.h - common kernel platform header
 *
 * DESCRIPTION:
 *     This header file provides the Platform-dependent macros. Suggest to add
 * all the platform-dependent control logic here rather than in other files,
 * especially no to add the platform-dependent control logic in the other *.c
 * files.
 *
 ******************************************************************************/

/* ========== */
#define IS_MT6893(id) ((id) == 0x6893)
#define IS_MT6885(id) ((id) == 0x6885)
#define IS_MT6877(id) ((id) == 0x6877)
#define IS_MT6873(id) ((id) == 0x6873)
#define IS_MT6855(id) ((id) == 0x6855)
#define IS_MT6853(id) ((id) == 0x6853)
#define IS_MT6835(id) ((id) == 0x6835)
#define IS_MT6833(id) ((id) == 0x6833)
#define IS_MT6781(id) ((id) == 0x6781)
#define IS_MT6789(id) ((id) == 0x6789)

/* ========== */
#define IS_MT6779(id) ((id) == 0x6779)

/* ========== */
#define IS_MT6785(id) ((id) == 0x6785)

/* ========== */
#define IS_MT6768(id) ((id) == 0x6768)
#define IS_MT6765(id) ((id) == 0x6765)

/* ========== */
#define IS_MT6761(id) ((id) == 0x6761)
#define IS_MT6739(id) ((id) == 0x6739)

/* ========== */
#define IS_MT6580(id) ((id) == 0x6580)


/* =============================================================================
 * Must add the platform id in the one of the following macros by raw number.
 *   - IS_3RAW_PLATFORM: The platform exists 3 RAW.
 *   - IS_2RAW_PLATFORM: The platform exists only 2 RAW.
 */
#define IS_3RAW_PLATFORM(id)	(			\
				IS_MT6893(id) ||	\
				IS_MT6885(id) ||	\
				IS_MT6873(id))

#define IS_2RAW_PLATFORM(id)	(			\
				IS_MT6877(id) ||	\
				IS_MT6855(id) ||	\
				IS_MT6853(id) ||	\
				IS_MT6835(id) ||	\
				IS_MT6833(id) ||	\
				IS_MT6781(id) ||	\
				IS_MT6789(id))


/* =============================================================================
 * Must add the platform id in the following macro, if "CAMSV_TOP0 exists" and
 * "its capability is powerful enough to support stagger feature".
 */
#define IS_CAMSV_TOP0_AVAILABLE(id)	(			\
					IS_MT6893(id) ||	\
					IS_MT6885(id) ||	\
					IS_MT6877(id) ||	\
					IS_MT6873(id))

#define IS_CAMSV_TOP0_NOT_AVAILABLE(id)	(!(IS_CAMSV_TOP0_AVAILABLE(id)))

/* =============================================================================
 * Must add the platform id in the one of the following isp6s version macro.
 *   - isp6s_v1: mt6885-like platform
 *   - isp6s_v2: mt6873-like platform
 *   - isp6s_v3: mt6853-like platform
 */
#define IS_ISP6S_V1(id)	(IS_MT6893(id) || IS_MT6885(id))

#define IS_ISP6S_V2(id)	IS_MT6873(id)

#define IS_ISP6S_V3(id) (			\
			IS_MT6877(id) ||	\
			IS_MT6855(id) ||	\
			IS_MT6853(id) ||	\
			IS_MT6835(id) ||	\
			IS_MT6833(id) ||	\
			IS_MT6781(id) ||	\
			IS_MT6789(id))


/* =============================================================================
 * Must add the platform id in the following macro, if the platform supports
 * CCU.
 */
#define IS_CCU_AVAILABLE(id)	(			\
				IS_MT6893(id) ||	\
				IS_MT6885(id) ||	\
				IS_MT6877(id) ||	\
				IS_MT6873(id) ||	\
				IS_MT6853(id) ||	\
				IS_MT6833(id))


/* =============================================================================
 * Must add the platform id in the following macro, if the platform has to
 * control the clock, "CAMSYS_LARB14_CGPDN".
 */
#define IS_LARB14_CGPDN_NECESSARY(id)	(			\
					IS_MT6893(id) ||	\
					IS_MT6885(id) ||	\
					IS_MT6877(id) ||	\
					IS_MT6873(id) ||	\
					IS_MT6853(id) ||	\
					IS_MT6833(id))


/* =============================================================================
 * Must add the platform id in the following macro, if the platform has to
 * control the clock, "CAMSYS_LARB15_CGPDN".
 */
#define IS_LARB15_CGPDN_NECESSARY(id)	(IS_ISP6S_V1(id))


/* =============================================================================
 * Must add the platform id in the following macro, if the platform has to
 * control the clock, "CAMSYS_MAIN_CAM2MM_GALS_CGPDN".
 */
#define IS_CAM2MM_GALS_CGPDN_NECESSARY(id) (IS_ISP6S_V2(id) || IS_ISP6S_V3(id))


/* =============================================================================
 * The camera of mt6893/mt6885 uses 2 slave_common, For mtk_icc_get() API,
 *   Larb13/16/18 must use slave_common(0);
 *   Larb14/17 must use slave_common(1).
 * This information is described in the dts node, "iommu[X]".
 *
 * So, mt6893/mt6885 camera needs the 2nd slave common id.
 * For other platforms, the camera only uses single iommu. No need to use
 * SLAVE_COMMON(1).
 */
#define SET_2ND_SLAVE_COMMON(id, slave_common_id_2nd) ({	\
								\
	if (IS_ISP6S_V1(id)) {					\
		slave_common_id_2nd = SLAVE_COMMON(1);		\
	} else {						\
		slave_common_id_2nd = SLAVE_COMMON(0);		\
	}							\
})

/* =============================================================================
 * Get platform id from dts node with "compatible_name".
 * The platform id is got from the member, "mediatek,platform".
 * Must add the platform id in the following macro.
 */
#define GET_PLATFORM_ID(compatible_name) ({					\
	struct device_node *dev_node = NULL;					\
	const char *platform_id_str;						\
	unsigned int platform_id = 0x0;						\
										\
	dev_node = of_find_compatible_node(NULL, NULL, compatible_name);	\
	if (!dev_node) {							\
		LOG_NOTICE("Found no %s\n", compatible_name);			\
	} else {								\
		if (of_property_read_string(dev_node, "mediatek,platform",	\
			&platform_id_str) < 0) {				\
			LOG_NOTICE("no mediatek,platform name\n");		\
		} else {							\
			if (strncmp(platform_id_str, "mt6893", 6) == 0)		\
				platform_id = 0x6893;				\
			else if (strncmp(platform_id_str, "mt6885", 6) == 0)	\
				platform_id = 0x6885;				\
			else if (strncmp(platform_id_str, "mt6877", 6) == 0)	\
				platform_id = 0x6877;				\
			else if (strncmp(platform_id_str, "mt6873", 6) == 0)	\
				platform_id = 0x6873;				\
			else if (strncmp(platform_id_str, "mt6855", 6) == 0)	\
				platform_id = 0x6855;				\
			else if (strncmp(platform_id_str, "mt6853", 6) == 0)	\
				platform_id = 0x6853;				\
			else if (strncmp(platform_id_str, "mt6835", 6) == 0)	\
				platform_id = 0x6835;				\
			else if (strncmp(platform_id_str, "mt6833", 6) == 0)	\
				platform_id = 0x6833;				\
			else if (strncmp(platform_id_str, "mt6781", 6) == 0)	\
				platform_id = 0x6781;				\
			else if (strncmp(platform_id_str, "mt6789", 6) == 0)	\
				platform_id = 0x6789;				\
			else if (strncmp(platform_id_str, "mt6779", 6) == 0)	\
				platform_id = 0x6779;				\
			else if (strncmp(platform_id_str, "mt6785", 6) == 0)	\
				platform_id = 0x6785;				\
			else if (strncmp(platform_id_str, "mt6768", 6) == 0)	\
				platform_id = 0x6768;				\
			else if (strncmp(platform_id_str, "mt6765", 6) == 0)	\
				platform_id = 0x6765;				\
			else if (strncmp(platform_id_str, "mt6761", 6) == 0)	\
				platform_id = 0x6761;				\
			else if (strncmp(platform_id_str, "mt6739", 6) == 0)	\
				platform_id = 0x6739;				\
			else if (strncmp(platform_id_str, "mt6580", 6) == 0)	\
				platform_id = 0x6580;				\
		}								\
	}									\
	platform_id;								\
})
