// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
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


#define CONFIG_MTK_QOS_SUPPORT

#ifdef CONFIG_MTK_QOS_SUPPORT
#include <mmdvfs_pmqos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <smi_port.h>
#else
#include <mmdvfs_mgr.h>
#endif

#define MyTag "[ISP]"

#define LOG_VRB(format, args...) \
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


#ifdef CONFIG_MTK_QOS_SUPPORT
	struct plist_head gBW_LIST[ISP_IRQ_TYPE_INT_CAM_C_ST + 1];
	struct plist_head _gSVBW_LIST[ISP_IRQ_TYPE_INT_CAMSV_5_ST -
		ISP_IRQ_TYPE_INT_CAMSV_0_ST + 1];
	#define gSVBW_LIST(module) ({ \
		struct plist_head *ptr = NULL; \
		if (module >= ISP_IRQ_TYPE_INT_CAMSV_0_ST) { \
			ptr = &_gSVBW_LIST[module - \
				ISP_IRQ_TYPE_INT_CAMSV_0_ST]; \
		} else { \
			LOG_NOTICE("sv idx violation , force to ke\n"); \
		} \
		ptr; \
	})

	struct mm_qos_request gCAM_BW_REQ[ISP_IRQ_TYPE_INT_CAM_C_ST + 1]
			[_cam_max_];

	struct mm_qos_request _gSV_BW_REQ[ISP_IRQ_TYPE_INT_CAMSV_5_ST -
			ISP_IRQ_TYPE_INT_CAMSV_0_ST + 1]
			[_camsv_max_];

	#define gSV_BW_REQ(module, port) ({ \
		struct mm_qos_request *ptr = NULL; \
		if (module >= ISP_IRQ_TYPE_INT_CAMSV_0_ST) { \
			ptr = &_gSV_BW_REQ \
				[module - ISP_IRQ_TYPE_INT_CAMSV_0_ST][port]; \
		} else { \
			LOG_NOTICE("sv idx violation , force to ke\n"); \
		} \
		ptr; \
	})

	struct mtk_pm_qos_request isp_qos;
#else //CONFIG_MTK_QOS_SUPPORT

	struct mmdvfs_pm_qos_request isp_qos;

#endif

static u32 target_clk;

#ifdef CONFIG_MTK_QOS_SUPPORT
	inline void mtk_pmqos_remove(
		enum ISP_IRQ_TYPE_ENUM module)
	{
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_B_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_remove_all_request(&gBW_LIST[module]);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			mm_qos_remove_all_request(gSVBW_LIST(module));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
	}

	inline void mtk_pmqos_add(
		enum ISP_IRQ_TYPE_ENUM module,
		u32 portID)
	{
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			switch (portID) {
			case _camsv_imgo_:
				mm_qos_add_request(
					gSVBW_LIST(module),
					gSV_BW_REQ(module, portID),
					SMI_SOC0);
				break;
			default:
				LOG_NOTICE("unsupported port:%d\n", portID);
				break;
			}
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
				switch (portID) {
				case _camsv_imgo_:
					mm_qos_add_request(
						gSVBW_LIST(module),
						gSV_BW_REQ(module, portID),
						SMI_SOC1);
					break;
				default:
					LOG_NOTICE("unsupported port:%d\n",
						portID);
					break;
				}
				break;
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
				LOG_NOTICE("unsupported module:%d\n", module);
				break;
		default:
				switch (portID) {
				case _imgo_:
					mm_qos_add_request(
						&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_IMGO);
					break;
				case _rrzo_:
					mm_qos_add_request(
						&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_RRZO);
					break;
				case _ufeo_:
					mm_qos_add_request(
						&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_UFEO);
					break;
				case _aao_:
					mm_qos_add_request(
						&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_AAO);
					break;
				case _afo_:
					if (module ==
						ISP_IRQ_TYPE_INT_CAM_C_ST)
						mm_qos_add_request(
							&gBW_LIST[module],
							&gCAM_BW_REQ[module]
								[portID],
							SMI_AFO_1);
					else
						mm_qos_add_request(
							&gBW_LIST[module],
							&gCAM_BW_REQ[module]
								[portID],
							SMI_AFO);
					break;
				case _lcso_:
					mm_qos_add_request(
					&gBW_LIST[module],
						&gCAM_BW_REQ[module]
							[portID],
						SMI_LCSO);
					break;
				case _pdo_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_PDO);
					break;
				case _lmvo_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_LMVO);
					break;
				case _flko_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_FLKO);
					break;
				case _rsso_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_RSSO_A);
					break;
				case _pso_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_PSO);
					break;
				case _ufgo_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_UFGO);
					break;
				case _bpci_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_BPCI);
					break;
				case _lsci_:
					switch (module) {
					case ISP_IRQ_TYPE_INT_CAM_A_ST:
						mm_qos_add_request(
							&gBW_LIST[module],
							&gCAM_BW_REQ[module]
								[portID],
							SMI_LSCI_0);
						break;
					case ISP_IRQ_TYPE_INT_CAM_B_ST:
						mm_qos_add_request(
							&gBW_LIST[module],
							&gCAM_BW_REQ[module]
								[portID],
							SMI_LSCI_1);
						break;
					case ISP_IRQ_TYPE_INT_CAM_C_ST:
						mm_qos_add_request(
						&gBW_LIST[module],
						&gCAM_BW_REQ[module]
							[portID],
						SMI_LSCI_2);
						break;
					default:
						LOG_NOTICE(
							"unsupported module:%d\n",
							module);
						break;
					}
					break;
				case _rawi_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_RAWI_A);
					break;
				case _pdi_:
					mm_qos_add_request(&gBW_LIST[module],
						&gCAM_BW_REQ[module][portID],
						SMI_PDI);
					break;
				default:
					LOG_NOTICE("unsupported port:%d\n",
						portID);
					break;
				}
				break;
		}
	}

	inline void mtk_pmqos_set(
		enum ISP_IRQ_TYPE_ENUM module,
		u32 portID,
		struct ISP_BW bw)
	{
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_B_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			switch (portID) {
			case _ufeo_: /* FALLTHROUGH */
			case _ufgo_:
				mm_qos_set_request(
					&gCAM_BW_REQ[module][portID],
					bw.avg, bw.peak, BW_COMP_DEFAULT);
				break;
			case _imgo_:	/* FALLTHROUGH */
			case _rrzo_:	/* FALLTHROUGH */
			case _aao_:	/* FALLTHROUGH */
			case _afo_:	/* FALLTHROUGH */
			case _lcso_:	/* FALLTHROUGH */
			case _pdo_:	/* FALLTHROUGH */
			case _lmvo_:	/* FALLTHROUGH */
			case _flko_:	/* FALLTHROUGH */
			case _rsso_:	/* FALLTHROUGH */
			case _pso_:	/* FALLTHROUGH */
			case _bpci_:	/* FALLTHROUGH */
			case _lsci_:	/* FALLTHROUGH */
			case _rawi_:	/* FALLTHROUGH */
			case _pdi_:
				mm_qos_set_request(
					&gCAM_BW_REQ[module][portID],
					bw.avg, bw.peak, BW_COMP_NONE);
				break;
			default:
				LOG_NOTICE("unsupported port:%d\n", portID);
				break;
			}
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			switch (portID) {
			case _camsv_imgo_:
				mm_qos_set_request(
					gSV_BW_REQ(module, portID),
					bw.avg, bw.peak, BW_COMP_DEFAULT);
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

	inline void mtk_pmqos_update(
		enum ISP_IRQ_TYPE_ENUM module)
	{
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_B_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_update_all_request(&gBW_LIST[module]);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST: /* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			mm_qos_update_all_request(gSVBW_LIST(module));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
	}

	inline void mtk_pmqos_clr(
		enum ISP_IRQ_TYPE_ENUM module)
	{
		switch (module) {
		case ISP_IRQ_TYPE_INT_CAM_A_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_B_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAM_C_ST:
			mm_qos_update_all_request_zero(&gBW_LIST[module]);
			break;
		case ISP_IRQ_TYPE_INT_CAMSV_0_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_1_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_2_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_3_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_4_ST:
			/* FALLTHROUGH */
		case ISP_IRQ_TYPE_INT_CAMSV_5_ST:
			mm_qos_update_all_request_zero(gSVBW_LIST(module));
			break;
		default:
			LOG_NOTICE("unsupported module:%d\n", module);
			break;
		}
	}

	inline void mtk_dfs_add(void)
	{
		mtk_pm_qos_add_request(&isp_qos, PM_QOS_CAM_FREQ, 0);
	}
	inline void mtk_dfs_remove(void)
	{
		mtk_pm_qos_remove_request(&isp_qos);
	}
	inline void  mtk_dfs_clr(void)
	{
		mtk_pm_qos_update_request(&isp_qos, 0);
	}
	inline void  mtk_dfs_set(void) {}
	inline void mtk_dfs_update(u32 clk)
	{
		mtk_pm_qos_update_request(&isp_qos, clk);
	}
	inline void mtk_dfs_supported(u64 *frq, u32 *step)
	{
		mmdvfs_qos_get_freq_steps(PM_QOS_CAM_FREQ, frq, step);
	}
	inline unsigned int mtk_dfs_cur(void)
	{
		return mmdvfs_qos_get_freq(PM_QOS_CAM_FREQ);
	}

#else //#ifdef CONFIG_MTK_QOS_SUPPORT

	inline void mtk_pmqos_remove(
			enum ISP_IRQ_TYPE_ENUM module)
	{
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n");
	}
	inline void mtk_pmqos_add(
		enum ISP_IRQ_TYPE_ENUM module,
		u32 portID)
	{
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n");
	}
	inline void mtk_pmqos_set(
		enum ISP_IRQ_TYPE_ENUM module,
		u32 portID,
		struct ISP_BW bw)
	{
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n");
	}
	inline void mtk_pmqos_update(
		enum ISP_IRQ_TYPE_ENUM module)
	{
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n");
	}
	inline void mtk_pmqos_clr(
		enum ISP_IRQ_TYPE_ENUM module)
	{
		LOG_NOTICE("MTK_SET_PM_QOS is not supported\n");
	}

	inline void mtk_dfs_add(void)
	{
		//mmdvfs_pm_qos_add_request(&isp_qos,
		//	MMDVFS_PM_QOS_SUB_SYS_CAMERA, 0);
		LOG_NOTICE("mmdvfs_pm_qos_add_request is not supported\n");
	}
	inline void mtk_dfs_remove(void)
	{
		//mmdvfs_pm_qos_remove_request(&isp_qos);
		LOG_NOTICE("mtk_dfs_remove is not supported\n");
	}
	inline void mtk_dfs_clr(void)
	{
		//mmdvfs_pm_qos_update_request(&isp_qos,
		//	MMDVFS_PM_QOS_SUB_SYS_CAMERA, 0);
		LOG_NOTICE("mtk_dfs_clr is not supported\n");
	}
	inline void mtk_dfs_set(void) {}
	inline void mtk_dfs_update(u32 clk)
	{
		//mmdvfs_pm_qos_update_request(&isp_qos,
		//	MMDVFS_PM_QOS_SUB_SYS_CAMERA, clk);
		LOG_NOTICE("mtk_dfs_update is not supported\n");
	}
	inline void mtk_dfs_supported(u64 *frq, u32 *step)
	{
		//(*step) = mmdvfs_qos_get_thres_count(&isp_qos,
		//MMDVFS_PM_QOS_SUB_SYS_CAMERA);
		//for (u32 lv = 0; lv < *step; lv++) {
		//	frq[lv] = mmdvfs_qos_get_thres_value(&isp_qos,
		//		MMDVFS_PM_QOS_SUB_SYS_CAMERA, lv);
		//}
		LOG_NOTICE("mtk_dfs_supported is not supported\n");
	}
	inline unsigned void mtk_dfs_cur(void)
	{
		//return mmdvfs_qos_get_cur_thres(&isp_qos,
		//	MMDVFS_PM_QOS_SUB_SYS_CAMERA);
		LOG_NOTICE("mtk_dfs_cur is not supported\n");
	}
#endif

//#define EP_PMQOS
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

			plist_head_init(&gBW_LIST[module]);

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
			LOG_DBG("PM_QoS:m[%d]-bw_upd, bw(p=%d a=%d)MB/s\n",
					module, ptr[_rrzo_].peak,
					ptr[_rrzo_].avg);
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
			*pvalue = target_clk = 546;
			LOG_DBG("1:DFS Clk_0:%d", pvalue[0]);
			return 1;
#else
			u32 step, i = 0;
			u64 freq[ISP_CLK_LEVEL_CNT] = {0};

			mtk_dfs_supported(freq, &step);

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
		(module > ISP_IRQ_TYPE_INT_CAMSV_5_ST)) {
		LOG_NOTICE("supported only to SV0 to SV5\n");
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

			if (gSVBW_LIST(module))
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
			LOG_DBG("PM_QoS:m[%d]-bw_upd, bw(a=%d p=%d)MB/s\n",
					module, ptr[_camsv_imgo_].peak,
					ptr[_camsv_imgo_].avg);
		}
		break;
	case E_BW_CLR:
		if (pvalue[0] == MFALSE) {
			mtk_pmqos_clr(module);
		    LOG_INF("module:%d BW_clr\n", module);
		}
		break;
	case E_QOS_UNKNOWN:
	case E_CLK_ADD:
	case E_CLK_UPDATE:
	case E_CLK_CLR:
	case E_CLK_REMOVE:
	case E_CLK_SUPPORTED:
	case E_CLK_CUR:
	default:
		LOG_NOTICE("unsupport cmd:%d", cmd);
		Ret = -1;
		break;
	}
	return Ret;
}
