// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifdef CONFIG_COMPAT
	/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif


#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "inc/cam_qos.h"
//#include <linux/interconnect.h>
#include "mtk-interconnect.h"

#define CONFIG_MTK_QOS_SUPPORT_ENABLE
//#define EP_STAGE
//#define EP_PMQOS

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
#ifndef EP_STAGE
// #include <mmdvfs_pmqos.h>
#include <linux/pm_qos.h>

#ifdef CAMERA_ISP_MT6873
#include <mt6873-larb-port.h>
#include <dt-bindings/memory/mt6873-larb-port.h>
#elif defined(CAMERA_ISP_MT6853)
#include <mt6853-larb-port.h>
#include <dt-bindings/memory/mt6853-larb-port.h>
#endif

#include <dt-bindings/interconnect/mtk,mmqos.h>

#endif
#else
#ifndef EP_STAGE
#include <mmdvfs_mgr.h>
#endif
#endif

#define MyTag "[ISP]"

#define LOG_VRB(format, args...)   \
	pr_debug(MyTag "[%s] " format, __func__, ##args)

// #define ISP_DEBUG
#ifdef ISP_DEBUG
#define LOG_DBG(format, args...) \
	pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) \
	pr_info(MyTag "[%s] " format, __func__, ##args)

#define LOG_NOTICE(format, args...) \
	pr_notice(MyTag "[%s] " format, __func__, ##args)


#ifndef MTRUE
#define MTRUE	1
#endif
#ifndef MFALSE
#define MFALSE	  0
#endif

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE

struct icc_path *gCAM_BW_REQ[ISP_IRQ_TYPE_INT_CAM_C_ST + 1]
					 [_cam_max_];

struct icc_path *gSV_BW_REQ[ISP_IRQ_TYPE_INT_CAMSV_END_ST -
			ISP_IRQ_TYPE_INT_CAMSV_START_ST + 1]
					[_camsv_max_];

struct pm_qos_request isp_qos;

static struct device *mmdvfsDev;
static struct regulator *mmdvfsRegulator;
#else
#ifndef EP_STAGE
	struct mmdvfs_pm_qos_request isp_qos;
#endif
#endif

/* Unit of target_clk: MHz */
static u32 target_clk;

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE

void mtk_pmqos_remove(enum ISP_IRQ_TYPE_ENUM module)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		break;
#if (DISABLE_CAMSV_TOP0 == 0)
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
#endif
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

/* if module and portID are valid, return true. */
bool check_module_and_portID(enum ISP_IRQ_TYPE_ENUM module, u32 portID)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		if (portID >= _cam_max_) {
			LOG_NOTICE("CAM Wrong portID:%d\n", portID);
			return false;
		}
		break;
#if (DISABLE_CAMSV_TOP0 == 0)
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
#endif
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		if (portID >= _camsv_max_) {
			LOG_NOTICE("CAMSV Wrong portID:%d\n", portID);
			return false;
		}
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		return false;
	}

	return true;
}

void mtk_pmqos_add(struct device *dev, enum ISP_IRQ_TYPE_ENUM module, u32 portID)
{
	if (check_module_and_portID(module, portID) == false)
		return;

	switch (module) {

#if (DISABLE_CAMSV_TOP0 == 0)
#ifdef CAMERA_ISP_MT6873
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L14_CAM_CAMSV0),
				SLAVE_COMMON(0));
			break;
		case _camsv_ufeo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV1),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("modlue(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV2),
				SLAVE_COMMON(0));
			break;
		case _camsv_ufeo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV3),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("modlue(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
#elif defined(CAMERA_ISP_MT6853)
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L14_CAM_RESERVE4),
				SLAVE_COMMON(0));
			break;
		case _camsv_ufeo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_RESERVE1),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("modlue(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_RESERVE2),
				SLAVE_COMMON(0));
			break;
		case _camsv_ufeo_:
			gSV_BW_REQ[module][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_RESERVE3),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("modlue(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
#endif
#endif
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV4),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("modlue(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV5),
				SLAVE_COMMON(0));
			break;
		}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		switch (portID) {
		case _camsv_imgo_:
			gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID] = mtk_icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV6),
				SLAVE_COMMON(0));
			break;
		}
		break;
	default:
		switch (portID) {
		case _imgo_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_IMGO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_IMGO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_IMGO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _ltmso_:
		case _lcesho_:
		case _tsfso_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_LTMSO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_LTMSO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_LTMSO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _rrzo_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_RRZO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_RRZO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_RRZO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _lcso_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_LCESO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_LCESO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_LCESO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _aaho_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_AAHO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_AAHO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_AAHO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _aao_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_AAO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_AAO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_AAO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _flko_:
		case _pdo_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_FLKO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_FLKO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_FLKO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _afo_:
		case _lmvo_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_AFO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_AFO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_AFO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _rsso_:
		case _ufeo_:
		case _ufgo_:
		case _rsso_r2_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_RSSO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_RSSO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_RSSO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _crzo_:
		case _crzbo_:
		case _crzo_r2_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_CRZO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_CRZO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_CRZO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _yuvo_:
		case _yuvbo_:
		case _yuvco_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_YUVO_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_YUVO_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_YUVO_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _rawi_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_RAWI_R2_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_RAWI_R2_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_RAWI_R2_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _rawi_r3_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_RAWI_R3_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_RAWI_R3_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_RAWI_R3_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _bpci_:
		case _bpci_r2_:
		case _bpci_r3_:
		case _pdi_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_BPCI_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_BPCI_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_BPCI_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _cqi_r1_:
		case _cqi_r2_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_CQI_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_CQI_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_CQI_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _lsci_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_LSCI_R1_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_LSCI_R1_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_LSCI_R1_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		case _ufdi_r2_:
			switch (module) {
			case ISP_IRQ_TYPE_INT_CAM_A_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L16_CAM_UFDI_R2_A),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_B_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L17_CAM_UFDI_R2_B),
					SLAVE_COMMON(0));
				break;
			case ISP_IRQ_TYPE_INT_CAM_C_ST:
				gCAM_BW_REQ[module][portID] = mtk_icc_get(dev,
					MASTER_LARB_PORT(M4U_PORT_L18_CAM_UFDI_R2_C),
					SLAVE_COMMON(0));
				break;
			default:
				LOG_NOTICE("portID(%d): unsupported module:%d\n", portID, module);
				break;
			}
			break;
		default:
			LOG_NOTICE("module(%d): unsupported port:%d\n", module, portID);
			break;
		}
		break;
	}
}

void mtk_pmqos_set(enum ISP_IRQ_TYPE_ENUM module, u32 portID, struct ISP_BW bw)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		switch (portID) {
		case _ufeo_:
		case _ufgo_:
		case _ufdi_r2_:
		{//BW_COMP_DEFAULT
			int ret;

			ret = mtk_icc_set_bw(gCAM_BW_REQ[module][portID],
				MBps_to_icc(bw.avg), MBps_to_icc(bw.peak));

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(%d,%d) failed(%d)\n",
					MBps_to_icc(bw.avg), MBps_to_icc(bw.peak), ret);
		}
			break;
		case _imgo_:
		case _ltmso_:
		case _lcesho_:
		case _rrzo_:
		case _lcso_:
		case _aao_:
		case _aaho_:
		case _flko_:
		case _afo_:
		case _rsso_:
		case _lmvo_:
		case _yuvbo_:
		case _tsfso_:
		case _pdo_:
		case _crzo_:
		case _crzbo_:
		case _yuvco_:
		case _crzo_r2_:
		case _rsso_r2_:
		case _yuvo_:
		case _rawi_:
		case _rawi_r3_:
		case _bpci_:
		case _lsci_:
		case _bpci_r2_:
		case _bpci_r3_:
		case _pdi_:
		case _cqi_r1_:
		case _cqi_r2_:
		{//BW_COMP_NONE
			int ret;

			ret = mtk_icc_set_bw(gCAM_BW_REQ[module][portID],
				MBps_to_icc(bw.avg), MBps_to_icc(bw.peak));

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(%d,%d) failed(%d)\n",
					MBps_to_icc(bw.avg), MBps_to_icc(bw.peak), ret);
		}
			break;
		default:
			LOG_NOTICE("unsupported port:%d\n", portID);
		break;
	}
	break;
#if (DISABLE_CAMSV_TOP0 == 0)
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
#endif
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		switch (portID) {
		case _camsv_ufeo_:
		{
			//BW_COMP_DEFAULT
			int ret;

			ret = mtk_icc_set_bw(gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID],
				MBps_to_icc(bw.avg), MBps_to_icc(bw.peak));

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(%d,%d) failed(%d)\n",
					MBps_to_icc(bw.avg), MBps_to_icc(bw.peak), ret);
		}
			break;
		case _camsv_imgo_:
		{
			//BW_COMP_NONE
			int ret;

			ret = mtk_icc_set_bw(gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID],
				MBps_to_icc(bw.avg), MBps_to_icc(bw.peak));

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(%d,%d) failed(%d)\n",
					MBps_to_icc(bw.avg), MBps_to_icc(bw.peak), ret);
		}
			break;
		default:
			LOG_NOTICE("unsupported port:%d\n", portID);
			break;
		}
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

void mtk_pmqos_clr(enum ISP_IRQ_TYPE_ENUM module)
{
	unsigned short portID = 0;

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
	{
		int ret;

		for (portID = 0; portID < _cam_max_; portID++) {
			ret = mtk_icc_set_bw(gCAM_BW_REQ[module][portID], 0, 0);

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(0, 0) failed(%d)\n", ret);
		}
	}
		break;
#if (DISABLE_CAMSV_TOP0 == 0)
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
#endif
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
	{
		int ret;

		for (portID = _camsv_imgo_; portID < _camsv_max_; portID++) {
			ret = mtk_icc_set_bw(gSV_BW_REQ[module - ISP_IRQ_TYPE_INT_CAMSV_START_ST][portID], 0, 0);

			if (ret)
				LOG_NOTICE("mtk_icc_set_bw(0, 0) failed(%d)\n", ret);
		}
	}
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}


unsigned int mtk_dfs_get_cur_freq(void)
{
	unsigned long freq = 0;
	int volt = regulator_get_voltage(mmdvfsRegulator);
	struct dev_pm_opp *opp =
		dev_pm_opp_find_freq_ceil_by_volt(mmdvfsDev, volt);

	if (IS_ERR(opp))
		LOG_NOTICE("Error get current freq fail\n");
	else {
		freq = dev_pm_opp_get_freq(opp);
		dev_pm_opp_put(opp);
	}

	return (unsigned int)freq;
}

#define mtk_dfs_clr()	do { \
	int volt = 0, ret = 0; \
	ret = regulator_set_voltage(mmdvfsRegulator, volt, INT_MAX);\
	if (ret) \
		LOG_NOTICE("Error: E_CLK_UPDATE fail\n"); \
} while (0)

#define mtk_dfs_update(clk)	do { \
	struct dev_pm_opp *opp; \
	int volt = 0, ret = 0; \
	opp = dev_pm_opp_find_freq_ceil(mmdvfsDev, &clk); \
	if (IS_ERR(opp)) \
		opp = dev_pm_opp_find_freq_floor(mmdvfsDev, &clk); \
	volt = dev_pm_opp_get_voltage(opp); \
	dev_pm_opp_put(opp); \
	ret = regulator_set_voltage(mmdvfsRegulator, volt, INT_MAX);\
	if (ret) \
		LOG_NOTICE("Error: E_CLK_UPDATE fail\n"); \
} while (0)

#define mtk_dfs_supported(frq, step)	\
	mmdvfs_qos_get_freq_steps(PM_QOS_CAM_FREQ, frq, &step)

#define mtk_dfs_cur() mtk_dfs_get_cur_freq()

#endif

int ISP_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue)
{
	int Ret = 0;

	if (module > ISP_IRQ_TYPE_INT_CAM_C_ST) {
		LOG_NOTICE("supported only to CAM_C\n");
		return 1;
	}

	switch (cmd) {
	case E_BW_REMOVE:
		mtk_pmqos_remove(module);
		LOG_INF("PM_QOS:module:%d,OFF\n", module);
		break;
	case E_BW_ADD:
	{
		u32 i = 0;

		for (; i < _cam_max_; i++)
			mtk_pmqos_add(mmdvfsDev, module, i);

		LOG_INF("PM_QOS:module:%d,ON\n", module);
	}
		break;
	case E_BW_UPDATE:
	{
		u32 i = 0;

		struct ISP_BW *ptr;

		for (; i < _cam_max_; i++) {
			ptr = (struct ISP_BW *)pvalue;
			mtk_pmqos_set(module, i, ptr[i]);
		}
		LOG_INF(
			"PM_QoS: module[%d]-bw_update, bw(peak avg)(%d %d) MB/s\n",
			module, ptr[_rrzo_].peak, ptr[_rrzo_].avg);
	}
		break;
	case E_BW_CLR:
		if (pvalue[0] == MFALSE) {
			mtk_pmqos_clr(module);
			LOG_DBG("module:%d bw_clr\n", module);
		}
		break;
	case E_CLK_ADD:
		break;
	case E_CLK_REMOVE:
		break;
	case E_CLK_CLR:
		mtk_dfs_clr();
		LOG_DBG("DFS_clr\n");
		break;
	case E_CLK_UPDATE:
	{
		unsigned long freq = 0;

		freq = (*(u32 *)pvalue) * 1000000; /* MHz to Hz */
		mtk_dfs_update(freq);
		do_div(freq, 1000000); /* Hz to MHz*/
		target_clk = (u32)freq;
		LOG_INF("DFS Set clock :(%d, %d) MHz\n", *pvalue, target_clk);
	}
		break;
	case E_CLK_SUPPORTED:
	{
#ifdef EP_PMQOS
		*pvalue = target_clk = 624;

		LOG_DBG("1:DFS Clk_0:%d\n", pvalue[0]);
		return 1;
#else
		unsigned int num_available, i = 0;
		u32 *speeds;
		struct dev_pm_opp *opp;
		unsigned long freq;
		#define STR_SIZE (128)
		char str[STR_SIZE];
		int c_num = 0;
		int size_remain = STR_SIZE;

		/* number of available opp */
		num_available = dev_pm_opp_get_opp_count(mmdvfsDev);
		if (num_available > 0) {
			speeds = kcalloc(num_available, sizeof(u32),
				GFP_KERNEL);
			freq = 0;
			if (speeds) {
				while (!IS_ERR(opp =
					dev_pm_opp_find_freq_ceil(
						mmdvfsDev, &freq))) {
				/* available freq is stored in speeds[i].
				 * The clock freq should be stored from high to low in pvalue
				 * for user space usage.
				 */
					speeds[num_available - i - 1] = freq;
					freq++;
					i++;
					dev_pm_opp_put(opp);
				}
			}
		}
		for (i = 0; i < num_available; i++) {
			do_div(speeds[i], 1000000); /* Hz to MHz */
			pvalue[i] = speeds[i];
		}
		if (num_available > 0)
			target_clk = pvalue[num_available - 1];

		for (i = 0 ; i < num_available; i++) {
			int tmp = 0;

			tmp = snprintf(str + c_num, size_remain,
				"DFS Clk_%d:%d MHz\n", i, pvalue[i]);

			if (tmp < 0) {
				LOG_INF("snprintf failed\n");
				break;
			}
			c_num += tmp;
			size_remain -= tmp;

			if (size_remain <= 0) {
				LOG_INF("str size is not enough\n");
				break;
			}
		}
		LOG_INF("%s", str);

		return (int)num_available;
#endif
		}
		break;
	case E_CLK_CUR:
#ifdef EP_PMQOS
		pvalue[0] = (unsigned int)target_clk;
#else
		pvalue[0] = mtk_dfs_cur();
		/* Hz to MHz */
		do_div(pvalue[0], 1000000);
#endif
		pvalue[1] = (unsigned int)target_clk;
		LOG_INF("cur clk:%d MHz,tar clk:%d MHz\n", pvalue[0], pvalue[1]);
		break;
	case E_QOS_UNKNOWN:
	default:
		LOG_NOTICE("unsupport cmd:%d\n", cmd);
		Ret = -1;
		break;
	}

	return Ret;
}
EXPORT_SYMBOL_GPL(ISP_SetPMQOS);


int SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue)
{
	int Ret = 0;

	if ((module < ISP_IRQ_TYPE_INT_CAMSV_START_ST) ||
	    (module > ISP_IRQ_TYPE_INT_CAMSV_END_ST)) {
		LOG_NOTICE("supported only to SV0 to SV7\n");
		return 1;
	}

	switch (cmd) {
#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
	case E_BW_REMOVE:
		mtk_pmqos_remove(module);
		LOG_INF("PM_QOS:module:%d,OFF\n", module);
		break;
	case E_BW_ADD:
	{
		u32 i = 0;

		for (; i < _camsv_max_; i++)
			mtk_pmqos_add(mmdvfsDev, module, i);

		LOG_DBG("PM_QOS:module:%d,ON\n", module);
	}
		break;
	case E_BW_UPDATE:
	{
		u32 i = 0;
		struct ISP_BW *ptr;

		for (; i < _camsv_max_; i++) {
#if (DISABLE_CAMSV_TOP0 == 0)
			if ((i == _camsv_ufeo_) &&
				(module < ISP_IRQ_TYPE_INT_CAMSV_0_ST ||
				module > ISP_IRQ_TYPE_INT_CAMSV_1_ST))
				continue;
#else
			if (i == _camsv_ufeo_)
				continue;
#endif
			ptr = (struct ISP_BW *)pvalue;
			mtk_pmqos_set(module, i, ptr[i]);
		}
		LOG_DBG(
			"PM_QoS: module[%d]-bw_update, bw(peak avg)(%d %d) MB/s\n",
			module, ptr[_camsv_imgo_].peak,
			ptr[_camsv_imgo_].avg);
	}
		break;
	case E_BW_CLR:
		if (pvalue[0] == MFALSE)
			mtk_pmqos_clr(module);
		LOG_DBG("module:%d BW_clr\n", module);
		break;
	case E_CLK_UPDATE:
	{
		unsigned long freq;

		freq = (*(u32 *)pvalue) * 1000000; // MHz to Hz
		LOG_INF("E_CLK_UPDATE %d MHz\n", *pvalue);
		mtk_dfs_update(freq);
		do_div(freq, 1000000); // Hz to MHz
		target_clk = (u32)freq;
		LOG_INF("DFS Set clock :%d MHz\n", target_clk);
	}
		break;
	case E_CLK_CUR:
#ifdef EP_PMQOS
		pvalue[0] = (unsigned int)target_clk;
#else
		pvalue[0] = mtk_dfs_cur();
		/* Hz to MHz */
		do_div(pvalue[0], 1000000);
#endif
		pvalue[1] = (unsigned int)target_clk;
		LOG_INF("cur clk:%d MHz,tar clk:%d MHz\n", pvalue[0], pvalue[1]);
		break;
	case E_CLK_SUPPORTED:
	{
#ifdef EP_PMQOS
		*pvalue = target_clk = 624;

		LOG_DBG("1:DFS Clk_0:%d\n", pvalue[0]);
		return 1;
#else
		unsigned int num_available, i = 0;
		u32 *speeds;
		struct dev_pm_opp *opp;
		unsigned long freq;

		/* number of available opp */
		num_available = dev_pm_opp_get_opp_count(mmdvfsDev);
		if (num_available > 0) {
			speeds = kcalloc(num_available, sizeof(u32), GFP_KERNEL);

			freq = 0;
			if (speeds) {
				while (!IS_ERR(opp =
					dev_pm_opp_find_freq_ceil(
						mmdvfsDev, &freq))) {
				/* available freq is stored in speeds[i].
				 * The clock freq should be stored from high to low in pvalue
				 * for user space usage.
				 */
					speeds[num_available - i - 1] = freq;
					freq++;
					i++;
					dev_pm_opp_put(opp);
				}
			}
		}
		for (i = 0; i < num_available; i++) {
			do_div(speeds[i], 1000000); /* Hz to MHz */
			pvalue[i] = speeds[i];
		}
		if (num_available > 0)
			target_clk = pvalue[num_available - 1];

		for (i = 0 ; i < num_available; i++)
			LOG_INF("2:DFS Clk_%d:%d MHz\n", i, pvalue[i]);

		return (int)num_available;
#endif
	}
		break;
	case E_QOS_UNKNOWN:
	case E_CLK_ADD:
	case E_CLK_CLR:
	case E_CLK_REMOVE:
#endif
	default:
		LOG_NOTICE("unsupport cmd:%d\n", cmd);
		Ret = -1;
		break;
	}
	return Ret;
}
EXPORT_SYMBOL_GPL(SV_SetPMQOS);

static int cam_qos_probe(struct platform_device *pdev)
{
	LOG_INF("CAM QOS probe.\n");
#ifndef EP_STAGE
	dev_pm_opp_of_add_table(&pdev->dev);
	mmdvfsDev = &pdev->dev;
	mmdvfsRegulator = devm_regulator_get(&pdev->dev, "dvfsrc-vcore");
#endif
	return 0;
}

static const struct of_device_id of_match_cam_qos[] = {
	{ .compatible = "mediatek,cam_qos_legacy", },
	{}
};

static struct platform_driver cam_qos_drv = {
	.probe = cam_qos_probe,
	.driver = {
		.name = "cam_qos",
		.of_match_table = of_match_cam_qos,
	},
};

static int __init cam_qos_init(void)
{
	return platform_driver_register(&cam_qos_drv);
}

static void __exit cam_qos_exit(void)
{
	platform_driver_unregister(&cam_qos_drv);
}

/*******************************************************************************
 *
 ******************************************************************************/
module_init(cam_qos_init);
module_exit(cam_qos_exit);
MODULE_DESCRIPTION("Camera QOS");
MODULE_AUTHOR("SW7");
MODULE_LICENSE("GPL");
