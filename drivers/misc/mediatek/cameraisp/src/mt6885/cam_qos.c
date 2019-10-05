/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifdef CONFIG_COMPAT
	/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif


#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include "inc/cam_qos.h"

//#define CONFIG_MTK_QOS_SUPPORT_ENABLE
#define EP_STAGE

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
#ifndef EP_STAGE
#include <mmdvfs_pmqos.h>
#endif
#else
#ifndef EP_STAGE
#include <mmdvfs_mgr.h>
#endif
#endif

#define MyTag "[ISP]"

#define LOG_VRB(format, args...)   \
	pr_debug(MyTag "[%s] " format, __func__, ##args)

#define ISP_DEBUG
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
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
	struct plist_head gBW_LIST[ISP_IRQ_TYPE_INT_CAM_C_ST+1];
	struct plist_head _gSVBW_LIST
		[ISP_IRQ_TYPE_INT_CAMSV_7_ST+1 - ISP_IRQ_TYPE_INT_CAMSV_0_ST];

	#define gSVBW_LIST(module) ({ \
		struct plist_head *ptr = NULL; \
		if (module >= ISP_IRQ_TYPE_INT_CAMSV_0_ST) { \
			ptr = &_gSVBW_LIST \
				[module - ISP_IRQ_TYPE_INT_CAMSV_0_ST]; \
		} \
		else { \
			LOG_NOTICE("sv idx violation , force to ke\n"); \
		} \
		ptr; \
	})



	struct mm_qos_request gCAM_BW_REQ[ISP_IRQ_TYPE_INT_CAM_C_ST+1]
					 [_cam_max_];

	struct mm_qos_request _gSV_BW_REQ[ISP_IRQ_TYPE_INT_CAMSV_7_ST + 1 -
					  ISP_IRQ_TYPE_INT_CAMSV_0_ST]
					 [_camsv_max_];

	#define gSV_BW_REQ(module, port) ({ \
		struct mm_qos_request *ptr = NULL; \
		if (module >= ISP_IRQ_TYPE_INT_CAMSV_0_ST) { \
			ptr = &_gSV_BW_REQ\
				[module - ISP_IRQ_TYPE_INT_CAMSV_0_ST][port];\
		} \
		else { \
			LOG_NOTICE("sv idx violation , force to ke\n"); \
		} \
		ptr; \
	})



	struct pm_qos_request isp_qos;
#else
#ifndef EP_STAGE
	struct mmdvfs_pm_qos_request isp_qos;
#endif
#endif

//#ifndef EP_STAGE
static u32 target_clk;
//#endif

#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE

#if 1

void mtk_pmqos_remove(module)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		mm_qos_remove_all_request(&gBW_LIST[module]);
		break;
	case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
	case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
		mm_qos_remove_all_request(gSVBW_LIST(module));
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

void mtk_pmqos_add(module, portID)
{
switch (module) {
case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
	switch (portID) {
	case _camsv_imgo_:
		mm_qos_add_request(gSVBW_LIST(module),
				   gSV_BW_REQ(module, portID),
				   SMI_PORT_CAMSV_1);
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
		mm_qos_add_request(gSVBW_LIST(module),
				   gSV_BW_REQ(module, portID),
				   SMI_PORT_CAMSV_2);
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
		mm_qos_add_request(gSVBW_LIST(module),
				   gSV_BW_REQ(module, portID),
				   SMI_PORT_CAMSV_3);
		break;
	}
	break;
case ISP_IRQ_TYPE_INT_CAMSV_6_ST:
case ISP_IRQ_TYPE_INT_CAMSV_7_ST:
	LOG_NOTICE("unsupported module:%d\n", module);
	break;
default:
	switch (portID) {
	case _imgo_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_IMGO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_IMGO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_IMGO_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _ltmso_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LTMSO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LTMSO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LTMSO_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rrzo_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RRZO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RRZO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RRZO_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _lcso_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LCESO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LCESO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LCESO_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _aao_:
	case _aaho_
	case _tsfso_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AAO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AAO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AAO_R1_C);
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
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_FLKO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_FLKO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_FLKO_R1_C);
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
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AFO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AFO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_AFO_R1_C);
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
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RSSO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RSSO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RSSO_R1_C);
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
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_CRZO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_CRZO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_CRZO_R1_C);
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
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_YUVO_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_YUVO_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_YUVO_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _rawi_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RAWI_R2_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RAWI_R2_b);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_RAWI_R2_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _bpci_:
	case _bpci_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_BPCI_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_BPCI_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_BPCI_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _lsci_:
	case _pdi_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LSCI_R1_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LSCI_R1_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_LSCI_R1_C);
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
		break;
	case _ufdi_r2_:
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_UFDI_R2_A);
			break;
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_UFDI_R2_B);
			break;
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_add_request(&gBW_LIST[module],
				   &gCAM_BW_REQ[module][portID],
				   SMI_PORT_UFDI_R2_C);
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

void mtk_pmqos_set(module, portID, bw)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
		switch (portID) {
		case _ufeo_:
		case _ufgo_:
		case _ufdi_r2_:
		{
			mm_qos_set_request(
				&gCAM_BW_REQ[module][portID],
				bw.avg, bw.peak, BW_COMP_DEFAULT);
		}
			break;
		case _imgo_:
		case _ltmso_:
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
		case _bpci_:
		case _lsci_:
		case _bpci_r2_:
		case _pdi_:
		{
			mm_qos_set_request(
				&gCAM_BW_REQ[module][portID],
				bw.avg, bw.peak, BW_COMP_NONE);
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
			mm_qos_set_request(
				gSV_BW_REQ(module, portID),
				bw.avg, bw.peak, BW_COMP_DEFAULT);
			break;
		case _camsv_imgo_:
			mm_qos_set_request(
				gSV_BW_REQ(module, portID),
				bw.avg, bw.peak, BW_COMP_NONE);
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

void mtk_pmqos_update(module)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
	{
		mm_qos_update_all_request(&gBW_LIST[module]);
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
		mm_qos_update_all_request(gSVBW_LIST(module));
	}
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

void mtk_pmqos_clr(module)
{
	switch (module) {
	case ISP_IRQ_TYPE_INT_CAM_A_ST:
	case ISP_IRQ_TYPE_INT_CAM_B_ST:
	case ISP_IRQ_TYPE_INT_CAM_C_ST:
	{
		mm_qos_update_all_request_zero(&gBW_LIST[module]);
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
		mm_qos_update_all_request_zero(gSVBW_LIST(module));
	}
		break;
	default:
		LOG_NOTICE("unsupported module:%d\n", module);
		break;
	}
}

#else

#endif

	#define mtk_dfs_add()		\
		pm_qos_add_request(&isp_qos, PM_QOS_CAM_FREQ, 0)

	#define mtk_dfs_remove()	\
		pm_qos_remove_request(&isp_qos)
	#define mtk_dfs_clr()		\
		pm_qos_update_request(&isp_qos, 0)

	#define mtk_dfs_set()

	#define mtk_dfs_update(clk)	\
		pm_qos_update_request(&isp_qos, clk)

	#define mtk_dfs_supported(frq, step)	\
		mmdvfs_qos_get_freq_steps(PM_QOS_CAM_FREQ, frq, &step)

	#define mtk_dfs_cur()		\
		mmdvfs_qos_get_freq(PM_QOS_CAM_FREQ)
#else
	#define mtk_pmqos_remove(module)	\
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

	#define mtk_pmqos_add(module, portID)	\
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

	#define mtk_pmqos_set(module, portID, bw)	\
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

	#define mtk_pmqos_update(module)	\
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

	#define mtk_pmqos_clr(module)		\
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n")

#ifndef EP_STAGE
	#define mtk_dfs_add()		\
		mmdvfs_pm_qos_add_request(&isp_qos, \
					  MMDVFS_PM_QOS_SUB_SYS_CAMERA, 0)
	#define mtk_dfs_remove()	\
		mmdvfs_pm_qos_remove_request(&isp_qos)

	#define mtk_dfs_clr()		\
		mmdvfs_pm_qos_update_request(&isp_qos, \
					  MMDVFS_PM_QOS_SUB_SYS_CAMERA, 0)
	#define mtk_dfs_set()

	#define mtk_dfs_update(clk)	\
		mmdvfs_pm_qos_update_request(&isp_qos, \
					  MMDVFS_PM_QOS_SUB_SYS_CAMERA, clk)

	#define mtk_dfs_supported(frq, step) do { \
		step = mmdvfs_qos_get_thres_count(&isp_qos, \
					  MMDVFS_PM_QOS_SUB_SYS_CAMERA); \
		for (u32 lv = 0; lv < step; lv++) { \
			frq[lv] = mmdvfs_qos_get_thres_value(&isp_qos, \
					  MMDVFS_PM_QOS_SUB_SYS_CAMERA, lv); \
		} \
	} while (0)

	#define mtk_dfs_cur()     \
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

#define EP_PMQOS
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
//#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
	case E_BW_REMOVE:
		mtk_pmqos_remove(module);
		LOG_INF("PM_QOS:module:%d,OFF\n", module);
		break;
	case E_BW_ADD:
		{
			u32 i = 0;
			#ifdef CONFIG_MTK_QOS_SUPPORT_ENABLE
			plist_head_init(&gBW_LIST[module]);
			#endif
			for (; i < _cam_max_; i++)
				mtk_pmqos_add(module, i);

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
		mtk_dfs_set();
		target_clk = *(u32 *)pvalue;
		mtk_dfs_update(target_clk);
		LOG_DBG("DFS Set clock :%d", *pvalue);
		break;
	case E_CLK_SUPPORTED:
		{
#ifdef EP_PMQOS
			*pvalue = target_clk = 624;

			LOG_DBG("1:DFS Clk_0:%d", pvalue[0]);
			return 1;
#else
			u32 step, i = 0;
			u64 freq[ISP_CLK_LEVEL_CNT];

			mtk_dfs_supported(freq, step);
			for (i = 0; i < step; i++)
				pvalue[i] = freq[i];

			target_clk = pvalue[step - 1];

			for (i = 0 ; i < step; i++)
				LOG_DBG("2:DFS Clk_%d:%d", i, pvalue[i]);

			return (int)step;
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
		LOG_DBG("cur clk:%d,tar clk:%d", pvalue[0], pvalue[1]);
		break;
	case E_QOS_UNKNOWN:
//#endif
	default:
		LOG_NOTICE("unsupport cmd:%d", cmd);
		Ret = -1;
		break;
	}

	return Ret;
}


int SV_SetPMQOS(
	enum E_QOS_OP cmd,
	enum ISP_IRQ_TYPE_ENUM module,
	unsigned int *pvalue)
{
	int Ret = 0;

	if ((module < ISP_IRQ_TYPE_INT_CAMSV_0_ST) &&
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

			plist_head_init(gSVBW_LIST(module));
			for (; i < _camsv_max_; i++)
				mtk_pmqos_add(module, i);

			LOG_DBG("PM_QOS:module:%d,ON\n", module);
		}
		break;
	case E_BW_UPDATE:
		{
			u32 i = 0;
			struct ISP_BW *ptr;

			for (; i < _camsv_max_; i++) {
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
		mtk_dfs_set();
		target_clk = *(u32 *)pvalue;
		mtk_dfs_update(target_clk);
		LOG_INF("DFS Set clock :%d", *pvalue);
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
			u32 step, i = 0;
			u64 freq[ISP_CLK_LEVEL_CNT];

			mtk_dfs_supported(freq, step);
			for (i = 0; i < step; i++)
				pvalue[i] = freq[i];

			target_clk = pvalue[step - 1];

			for (i = 0 ; i < step; i++)
				LOG_INF("2:DFS Clk_%d:%d", i, pvalue[i]);

			return (int)step;
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

