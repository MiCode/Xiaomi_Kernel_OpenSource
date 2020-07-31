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
#include <linux/interconnect.h>

#define CONFIG_MTK_QOS_SUPPORT_ENABLE
//#define EP_STAGE
//#define EP_PMQOS

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
#ifndef EP_STAGE
// #include <mmdvfs_pmqos.h>
#include <linux/pm_qos.h>
#include <mt6873-larb-port.h>
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <dt-bindings/memory/mt6873-larb-port.h>

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

struct icc_path *gSV_BW_REQ[ISP_IRQ_TYPE_INT_CAMSV_7_ST + 1 -
					ISP_IRQ_TYPE_INT_CAMSV_0_ST]
					[_camsv_max_];

struct pm_qos_request isp_qos;

static struct device *mmdvfsDev;
static struct regulator *mmdvfsRegulator;
#else
#ifndef EP_STAGE
	struct mmdvfs_pm_qos_request isp_qos;
#endif
#endif

static u32 target_clk;

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE

void mtk_pmqos_remove(enum ISP_IRQ_TYPE_ENUM module)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
	//mm_qos_remove_all_request(&gBW_LIST[module]);
	LOG_NOTICE("common pmqos do not support remove all request");
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
	//mm_qos_remove_all_request(gSVBW_LIST(module));
	LOG_NOTICE("common pmqos do not support remove all request");
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

void mtk_pmqos_add(struct device *dev, enum ISP_IRQ_TYPE_ENUM module, u32 portID)
{
switch (module) {
case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	switch (portID) {
	case _camsv_imgo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L14_CAM_CAMSV0),
		SLAVE_COMMON(0));
		break;
	case _camsv_ufeo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV1),
		SLAVE_COMMON(0));
		break;
	default:
		LOG_NOTICE("unsupported port:%d\n", portID);
		break;
	}
	break;
case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	switch (portID) {
	case _camsv_imgo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV2),
		SLAVE_COMMON(0));
		break;
	case _camsv_ufeo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV3),
		SLAVE_COMMON(0));
		break;
	default:
		LOG_NOTICE("unsupported port:%d\n", portID);
		break;
	}
	break;
case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	switch (portID) {
	case _camsv_imgo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV4),
		SLAVE_COMMON(0));
		break;
	default:
		LOG_NOTICE("unsupported port:%d\n", portID);
		break;
	}
	break;
case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	switch (portID) {
	case _camsv_imgo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
		MASTER_LARB_PORT(M4U_PORT_L13_CAM_CAMSV5),
		SLAVE_COMMON(0));
		break;
	}
	break;
case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
	switch (portID) {
	case _camsv_imgo_:
	gSV_BW_REQ[module][portID] = icc_get(dev,
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
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_IMGO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_IMGO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_IMGO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _ltmso_:
	case _lcesho_:
	case _tsfso_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_LTMSO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_LTMSO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_LTMSO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rrzo_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_RRZO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_RRZO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_RRZO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _lcso_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_LCESO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_LCESO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_LCESO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _aaho_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_AAHO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_AAHO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_AAHO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _aao_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L16_CAM_AAO_R1_A),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L17_CAM_AAO_R1_B),
				SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			gCAM_BW_REQ[module][portID] = icc_get(dev,
				MASTER_LARB_PORT(M4U_PORT_L18_CAM_AAO_R1_C),
				SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _flko_:
	case _pdo_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_FLKO_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_FLKO_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_FLKO_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _afo_:
	case _lmvo_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_AFO_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_AFO_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_AFO_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rsso_:
	case _ufeo_:
	case _ufgo_:
	case _rsso_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_RSSO_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_RSSO_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_RSSO_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _crzo_:
	case _crzbo_:
	case _crzo_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_CRZO_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_CRZO_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_CRZO_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _yuvo_:
	case _yuvbo_:
	case _yuvco_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_YUVO_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_YUVO_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_YUVO_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rawi_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_RAWI_R2_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_RAWI_R2_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_RAWI_R2_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rawi_r3_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_RAWI_R3_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_RAWI_R3_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_RAWI_R3_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _bpci_:
	case _bpci_r2_:
	case _bpci_r3_:
	case _pdi_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_BPCI_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_BPCI_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_BPCI_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _cqi_r1_:
	case _cqi_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_CQI_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_CQI_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_CQI_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _lsci_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_LSCI_R1_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_LSCI_R1_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_LSCI_R1_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _ufdi_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L16_CAM_UFDI_R2_A),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L17_CAM_UFDI_R2_B),
			SLAVE_COMMON(0));
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
		gCAM_BW_REQ[module][portID] = icc_get(dev,
			MASTER_LARB_PORT(M4U_PORT_L18_CAM_UFDI_R2_C),
			SLAVE_COMMON(0));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	default:
		LOG_NOTICE("unsupported port:%d\n", portID);
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
			icc_set_bw(
				gCAM_BW_REQ[module][portID],
				Bps_to_icc(bw.avg), Bps_to_icc(bw.peak));
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
			icc_set_bw(
				gCAM_BW_REQ[module][portID],
				Bps_to_icc(bw.avg), Bps_to_icc(bw.peak));
		}
			break;
		default:
			LOG_NOTICE("unsupported port:%d\n", portID);
		break;
	}
	break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		switch (portID) {
		case _camsv_ufeo_:
			//BW_COMP_DEFAULT
			icc_set_bw(
				gSV_BW_REQ[module][portID],
				Bps_to_icc(bw.avg), Bps_to_icc(bw.peak));
			break;
		case _camsv_imgo_:
			//BW_COMP_NONE
			icc_set_bw(
				gSV_BW_REQ[module][portID],
				Bps_to_icc(bw.avg), Bps_to_icc(bw.peak));
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

void mtk_pmqos_update(enum ISP_IRQ_TYPE_ENUM module)
{
	LOG_NOTICE("unsupport update all request\n");
}

void mtk_pmqos_clr(enum ISP_IRQ_TYPE_ENUM module)
{
	unsigned short portID = 0;

	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
	{
		for (portID = 0; portID < _cam_max_; portID++) {
			icc_set_bw(
				gCAM_BW_REQ[module][portID], 0, 0);
		}
	}
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
	{
		for (portID = _camsv_imgo_; portID < _camsv_max_; portID++) {
			icc_set_bw(
				gSV_BW_REQ[module][portID], 0, 0);
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

#define mtk_dfs_add()		\
	LOG_NOTICE("mtk_dfs_add is not supported\n")

#define mtk_dfs_remove()	\
	pm_qos_remove_request(&isp_qos)
#define mtk_dfs_clr()		\
	pm_qos_update_request(&isp_qos, 0)

#define mtk_dfs_set()

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

#else
#define mtk_pmqos_remove(module)	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_pmqos_add(dev, module, portID)	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_pmqos_set(module, portID, bw)	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_pmqos_update(module)	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_pmqos_clr(module)		\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#ifndef EP_STAGE
#define mtk_dfs_add()		\
	LOG_NOTICE("mtk_dfs_add is not supported\n")

#define mtk_dfs_remove()	\
	mmdvfs_pm_qos_remove_request(&isp_qos)

#define mtk_dfs_clr()		\
	mmdvfs_pm_qos_update_request(&isp_qos, \
				  MMDVFS_PM_QOS_SUB_SYS_CAMERA, 0)
#define mtk_dfs_set()

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
		LOG_NOTICE("Error: E_CLK_UPDATE fail"); \
} while (0)

#define mtk_dfs_supported(frq, step) do { \
	step = mmdvfs_qos_get_thres_count(&isp_qos, \
			  MMDVFS_PM_QOS_SUB_SYS_CAMERA); \
	for (u32 lv = 0; lv < step; lv++) { \
		frq[lv] = mmdvfs_qos_get_thres_value(&isp_qos, \
			  MMDVFS_PM_QOS_SUB_SYS_CAMERA, lv); \
	} \
} while (0)

#define mtk_dfs_cur() \
		mmdvfs_qos_get_cur_thres(&isp_qos, \
					 MMDVFS_PM_QOS_SUB_SYS_CAMERA)
#else
#define mtk_dfs_add()	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_remove()	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_clr()	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_set()	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_update(clk)		\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_supported(frq, step)		\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#define mtk_dfs_cur()	\
	LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")
#endif
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
		mtk_pmqos_update(module);
		LOG_DBG(
			"PM_QoS: module[%d]-bw_update, bw(peak avg)(%d %d) MB/s\n",
			module, ptr[_rrzo_].peak, ptr[_rrzo_].avg);
	}
		break;
	case E_BW_CLR:
		if (pvalue[0] == MFALSE) {
			mtk_pmqos_clr(module);
			LOG_INF("module:%d bw_clr\n", module);
		}
		break;
	case E_CLK_ADD:
		mtk_dfs_add();
		LOG_INF("DFS_add\n");
		break;
	case E_CLK_REMOVE:
		mtk_dfs_remove();
		LOG_INF("DFS_remove\n");
		break;
	case E_CLK_CLR:
		mtk_dfs_clr();
		LOG_INF("DFS_clr\n");
		break;
	case E_CLK_UPDATE:
	{
		unsigned long freq = 0;

		LOG_INF("E_CLK_UPDATE %d", *pvalue);
		freq = *(u32 *)pvalue; //need to check unit
		mtk_dfs_update(freq);
		target_clk = (u32)freq;
		LOG_INF("DFS Set clock :%d", target_clk);
	}
		break;
	case E_CLK_SUPPORTED:
	{
#ifdef EP_PMQOS
		*pvalue = target_clk = 624;

		LOG_DBG("1:DFS Clk_0:%d", pvalue[0]);
		return 1;
#else
		unsigned int num_available, i = 0;
		u32 *speeds;
		struct dev_pm_opp *opp;
		unsigned long freq;

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
				/* available freq is stored in speeds[i]*/
					speeds[i] = freq;
					freq++;
					i++;
					dev_pm_opp_put(opp);
				}
			}
		}
		for (i = 0; i < num_available; i++)
			pvalue[i] = speeds[i];
			if (num_available > 0)
				target_clk = pvalue[num_available - 1];

		for (i = 0 ; i < num_available; i++)
			LOG_DBG("2:DFS Clk_%d:%d", i, pvalue[i]);

		return (int)num_available;
#endif
		}
		break;
	case E_CLK_CUR:
#ifdef EP_PMQOS
		pvalue[0] = (unsigned int)target_clk;
#else
		pvalue[0] = (unsigned int)mtk_dfs_cur();
#endif
		pvalue[1] = (unsigned int)target_clk;
		LOG_INF("cur clk:%d,tar clk:%d", pvalue[0], pvalue[1]);
		break;
	case E_QOS_UNKNOWN:
	default:
		LOG_NOTICE("unsupport cmd:%d", cmd);
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

	if ((module < ISP_IRQ_TYPE_INT_CAMSV_0_ST) ||
	(module > ISP_IRQ_TYPE_INT_CAMSV_7_ST)) {
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
			if ((i == _camsv_ufeo_) &&
				(module < ISP_IRQ_TYPE_INT_CAMSV_0_ST ||
				module > ISP_IRQ_TYPE_INT_CAMSV_1_ST))
				continue;

			ptr = (struct ISP_BW *)pvalue;
			mtk_pmqos_set(module, i, ptr[i]);
		}
		mtk_pmqos_update(module);
		LOG_DBG(
			"PM_QoS: module[%d]-bw_update, bw(peak avg)(%d %d) MB/s\n",
			module, ptr[_camsv_imgo_].peak,
			ptr[_camsv_imgo_].avg);
	}
		break;
	case E_BW_CLR:
		if (pvalue[0] == MFALSE)
			mtk_pmqos_clr(module);
		LOG_INF("module:%d BW_clr\n", module);
		break;
	case E_CLK_UPDATE:
	{
		unsigned long freq;

		freq = *(u32 *)pvalue; //need to check unit
		LOG_INF("E_CLK_UPDATE %d", *pvalue);
		mtk_dfs_update(freq);

		target_clk = (u32)freq;
		LOG_INF("DFS Set clock :%d", target_clk);
	}
		break;
	case E_CLK_CUR:
#ifdef EP_PMQOS
		pvalue[0] = (unsigned int)target_clk;
#else
		pvalue[0] = (unsigned int)mtk_dfs_cur();
#endif
		pvalue[1] = (unsigned int)target_clk;
		LOG_INF("cur clk:%d,tar clk:%d", pvalue[0], pvalue[1]);
		break;
	case E_CLK_SUPPORTED:
	{
#ifdef EP_PMQOS
		*pvalue = target_clk = 624;

		LOG_DBG("1:DFS Clk_0:%d", pvalue[0]);
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
				/* available freq is stored in speeds[i]*/
					speeds[i] = freq;
					freq++;
					i++;
					dev_pm_opp_put(opp);
				}
			}
		}
		for (i = 0; i < num_available; i++)
			pvalue[i] = speeds[i];
		if (num_available > 0)
			target_clk = pvalue[num_available - 1];

		for (i = 0 ; i < num_available; i++)
			LOG_INF("2:DFS Clk_%d:%d", i, pvalue[i]);

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
		LOG_NOTICE("unsupport cmd:%d", cmd);
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
	{ .compatible = "mediatek,cam_qos", },
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
