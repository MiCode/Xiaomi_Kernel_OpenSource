/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _LINUX_MTK_PM_QOS_H
#define _LINUX_MTK_PM_QOS_H


enum {
	MTK_PM_QOS_RESERVED = 0,

	MTK_PM_QOS_MEMORY_BANDWIDTH,
	MTK_PM_QOS_MEMORY_EXT_BANDWIDTH,
	MTK_PM_QOS_HRT_BANDWIDTH,
	MTK_PM_QOS_DDR_OPP,
	MTK_PM_QOS_VCORE_OPP,
	MTK_PM_QOS_SCP_VCORE_REQUEST,

	MTK_PM_QOS_VVPU_OPP,
	MTK_PM_QOS_VMDLA_OPP,

	PM_QOS_DISP_FREQ,
	PM_QOS_MDP_FREQ,
	PM_QOS_VDEC_FREQ,
	PM_QOS_VENC_FREQ,
	PM_QOS_IMG_FREQ,
	PM_QOS_CAM_FREQ,
	PM_QOS_DPE_FREQ,
	PM_QOS_MM0_BANDWIDTH_LIMITER,
	PM_QOS_MM1_BANDWIDTH_LIMITER,
	MTK_PM_QOS_CPU_MEMORY_BANDWIDTH,
	MTK_PM_QOS_GPU_MEMORY_BANDWIDTH,
	MTK_PM_QOS_OTHER_MEMORY_BANDWIDTH,
	MTK_PM_QOS_POWER_MODEL_DDR_REQUEST,
	MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST,
	MTK_PM_QOS_VCORE_DVFS_FORCE_OPP,
	MTK_PM_QOS_MEMORY_BANDWIDTH_TEST,

	/* insert new class ID */
	MTK_PM_QOS_NUM_CLASSES,
};

#define PM_QOS_ISP_HRT_BANDWIDTH MTK_PM_QOS_HRT_BANDWIDTH
#define PM_QOS_MM_MEMORY_BANDWIDTH MTK_PM_QOS_MEMORY_BANDWIDTH
#define PM_QOS_APU_MEMORY_BANDWIDTH MTK_PM_QOS_MEMORY_EXT_BANDWIDTH
#define MTK_PM_QOS_ISP_HRT_BANDWIDTH PM_QOS_ISP_HRT_BANDWIDTH
#define MTK_PM_QOS_MM_MEMORY_BANDWIDTH MTK_PM_QOS_MEMORY_BANDWIDTH

#define MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define MTK_PM_QOS_HRT_BANDWIDTH_DEFAULT_VALUE		0
#define MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE		16
#define MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE		16
#define MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE	16
#define MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE		3
#define MTK_PM_QOS_VMDLA_OPP_DEFAULT_VALUE		3
#define PM_QOS_MM_FREQ_DEFAULT_VALUE		0
#define PM_QOS_MM_BANDWIDTH_LIMITER_DEFAULT_VALUE	0
#define MTK_PM_QOS_POWER_MODEL_DDR_REQUEST_DEFAULT_VALUE	0
#define MTK_PM_QOS_POWER_MODEL_VCORE_REQUEST_DEFAULT_VALUE	0
#define MTK_PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE	32
#define PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE         0
#define PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE      0
#define PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE	0


#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6779)
enum ddr_opp {
	DDR_OPP_0 = 0,
	DDR_OPP_1,
	DDR_OPP_2,
	DDR_OPP_3,
	DDR_OPP_4,
	DDR_OPP_5,
	DDR_OPP_6,
	DDR_OPP_7,
	DDR_OPP_UNREQ = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
};

enum vcore_opp {
	VCORE_OPP_0 = 0,
	VCORE_OPP_1,
	VCORE_OPP_2,
	VCORE_OPP_3,
	VCORE_OPP_4,
	VCORE_OPP_5,
	VCORE_OPP_6,
	VCORE_OPP_7,
	VCORE_OPP_UNREQ = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
};
#endif

struct mtk_pm_qos_request {
	struct list_head list_node;
	struct plist_node node;
	unsigned int pm_qos_class;
	char owner[20];
};

#if IS_ENABLED(CONFIG_MTK_PMQOS)
void mtk_pm_qos_add_request(struct mtk_pm_qos_request *req,
	unsigned int mtk_pm_qos_class, s32 value);
void mtk_pm_qos_update_request(struct mtk_pm_qos_request *req,
	s32 new_value);
void mtk_pm_qos_remove_request(struct mtk_pm_qos_request *req);
int mtk_pm_qos_request(unsigned int mtk_pm_qos_class);
int mtk_pm_qos_add_notifier(unsigned int mtk_pm_qos_class,
	struct notifier_block *notifier);
int mtk_pm_qos_remove_notifier(unsigned int mtk_pm_qos_class,
	struct notifier_block *notifier);
int mtk_pm_qos_request(unsigned int pm_qos_class);
int mtk_pm_qos_request_active(struct mtk_pm_qos_request *req);
#else
static inline void mtk_pm_qos_add_request(struct mtk_pm_qos_request *req,
	unsigned int mtk_pm_qos_class, s32 value)
{
}
static inline void mtk_pm_qos_update_request(
	struct mtk_pm_qos_request *req, s32 new_value)
{
}
static inline void mtk_pm_qos_remove_request(struct mtk_pm_qos_request *req)
{
}

static inline int mtk_pm_qos_request(unsigned int mtk_pm_qos_class)
{
	return 0;
}
static inline int mtk_pm_qos_add_notifier(unsigned int mtk_pm_qos_class,
	struct notifier_block *notifier)
{
	return 0;
}

static inline int mtk_pm_qos_remove_notifier(unsigned int mtk_pm_qos_class,
	struct notifier_block *notifier)
{
	return 0;
}

static inline int mtk_pm_qos_request_active(struct mtk_pm_qos_request *req)
{
	return 0;
}

#endif

#endif

