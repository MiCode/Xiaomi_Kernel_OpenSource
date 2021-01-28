// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "ddp_hal.h"
#include "ddp_reg.h"
#include "disp_pm_qos.h"
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include "layering_rule.h"
#include "mmprofile.h"
#include "mmprofile_function.h"
#include "disp_drv_log.h"
#include "disp_drv_platform.h"
#ifdef MTK_FB_MMDVFS_SUPPORT
#include "mmdvfs_pmqos.h"
#include "mt6765/smi_port.h"
#endif
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#define OCCUPIED_BW_RATIO 1330

#ifdef MTK_FB_MMDVFS_SUPPORT
static struct plist_head bw_request_list;  /* all module list */
static struct mm_qos_request ovl0_request;
static struct mm_qos_request ovl0_fbdc_request;
static struct mm_qos_request ovl0_2l_request;
static struct mm_qos_request ovl0_2l_fbdc_request;
static struct mm_qos_request rdma0_request;
static struct mm_qos_request wdma0_request;

static struct pm_qos_request ddr_opp_request;
static struct pm_qos_request mm_freq_request;

static struct plist_head hrt_request_list;
static struct mm_qos_request ovl0_hrt_request;
static struct mm_qos_request ovl0_2l_hrt_request;
static struct mm_qos_request rdma0_hrt_request;
static struct mm_qos_request wdma0_hrt_request;
static struct mm_qos_request hrt_bw_request;
#endif

static unsigned int has_hrt_bw;
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
#ifdef MTK_FB_MMDVFS_SUPPORT
static enum ddr_opp __remap_to_opp(enum HRT_LEVEL hrt)
{
	enum ddr_opp opp = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;

	switch (hrt) {
	case HRT_LEVEL_LEVEL0:
		opp = DDR_OPP_4; /* LP4-1200 */
		break;
	case HRT_LEVEL_LEVEL1:
		opp = DDR_OPP_2; /* LP4-2100 */
		break;
	case HRT_LEVEL_LEVEL2:
		opp = DDR_OPP_1; /* LP4-2800 */
		break;
	case HRT_LEVEL_LEVEL3:
		opp = DDR_OPP_0; /* LP4-3200 */
		break;
	case HRT_LEVEL_DEFAULT:
		opp = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;
		break;
	default:
		DISPERR("%s:unknown hrt level:%d\n", __func__, hrt);
		break;
	}

	return opp;
}

static int __init_cmdq_slots(cmdqBackupSlotHandle *pSlot,
	int count, int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);

	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);

	return 0;
}
#endif

static int __get_cmdq_slots(cmdqBackupSlotHandle Slot,
	unsigned int slot_index, unsigned int *value)
{
	int ret;

	ret = cmdqBackupReadSlot(Slot, slot_index, value);

	/* cmdq get slot fail */
	if (ret)
		DISPERR("DISP CMDQ get slot failed:%d\n", ret);

	return ret;
}

void disp_pm_qos_init(void)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long bandwidth;

	/* initialize display slot */
	__init_cmdq_slots(&(dispsys_slot), DISP_SLOT_NUM, 0);

	/* Initialize owner list */
	plist_head_init(&bw_request_list);

	mm_qos_add_request(&bw_request_list, &ovl0_request,
			   SMI_DISP_OVL0);
	mm_qos_add_request(&bw_request_list, &ovl0_fbdc_request,
			   SMI_DISP_OVL0);
	mm_qos_add_request(&bw_request_list, &ovl0_2l_request,
			   SMI_DISP_OVL0_2L);
	mm_qos_add_request(&bw_request_list, &ovl0_2l_fbdc_request,
			   SMI_DISP_OVL0_2L);
	mm_qos_add_request(&bw_request_list, &rdma0_request,
			   SMI_DISP_RDMA0);
	mm_qos_add_request(&bw_request_list, &wdma0_request,
			   SMI_DISP_WDMA0);
	pm_qos_add_request(&ddr_opp_request, MTK_PM_QOS_DDR_OPP,
			   MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	pm_qos_add_request(&mm_freq_request, PM_QOS_DISP_FREQ,
			   PM_QOS_MM_FREQ_DEFAULT_VALUE);

	plist_head_init(&hrt_request_list);

	mm_qos_add_request(&hrt_request_list, &ovl0_hrt_request,
			   SMI_DISP_OVL0);
	mm_qos_add_request(&hrt_request_list, &ovl0_2l_hrt_request,
			   SMI_DISP_OVL0_2L);
	mm_qos_add_request(&hrt_request_list, &rdma0_hrt_request,
			   SMI_DISP_RDMA0);
	mm_qos_add_request(&hrt_request_list, &wdma0_hrt_request,
			   SMI_DISP_WDMA0);
	mm_qos_add_request(&hrt_request_list, &hrt_bw_request,
			   get_virtual_port(VIRTUAL_DISP));

	disp_pm_qos_set_default_bw(&bandwidth);
	disp_pm_qos_set_default_hrt();
#endif
}

void disp_pm_qos_deinit(void)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	mm_qos_remove_all_request(&bw_request_list);
	pm_qos_remove_request(&ddr_opp_request);
	pm_qos_remove_request(&mm_freq_request);
#endif
}

int disp_pm_qos_request_dvfs(enum HRT_LEVEL hrt)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	enum ddr_opp opp = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;

	opp = __remap_to_opp(hrt);
	mmprofile_log_ex(ddp_mmp_get_events()->dvfs, MMPROFILE_FLAG_START,
			 hrt, opp);
	pm_qos_update_request(&ddr_opp_request, opp);
	mmprofile_log_ex(ddp_mmp_get_events()->dvfs, MMPROFILE_FLAG_END, 0, 0);
#endif
	return 0;
}

static int __set_hrt_bw(enum DISP_MODULE_ENUM module,
	unsigned int bandwidth)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	struct mm_qos_request *request;

	switch (module) {
	case DISP_MODULE_OVL0:
		request = &ovl0_hrt_request;
		break;
	case DISP_MODULE_OVL0_2L:
		request = &ovl0_2l_hrt_request;
		break;
	case DISP_MODULE_RDMA0:
		request = &rdma0_hrt_request;
		break;
	case DISP_MODULE_WDMA0:
		request = &wdma0_hrt_request;
		break;
	default:
		DISPERR("unsupport module id %s(%d)\n",
			ddp_get_module_name(module), module);
		return -1;
	}

	mm_qos_set_hrt_request(request, bandwidth);
#endif
	return 0;
}

static int __set_bw(enum DISP_MODULE_ENUM module,
	unsigned int bandwidth)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	struct mm_qos_request *request;

	switch (module) {
	case DISP_MODULE_OVL0:
		request = &ovl0_request;
		break;
	case DISP_MODULE_OVL0_2L:
		request = &ovl0_2l_request;
		break;
	case DISP_MODULE_RDMA0:
		request = &rdma0_request;
		break;
	case DISP_MODULE_WDMA0:
		request = &wdma0_request;
		break;
	default:
		DISPERR("unsupport module id %s(%d)\n",
			ddp_get_module_name(module), module);
		return -1;
	}
	bandwidth = bandwidth * OCCUPIED_BW_RATIO / 1000;
	mm_qos_set_bw_request(request, bandwidth, BW_COMP_NONE);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_PULSE,
			module, bandwidth);

	return 0;
}

static int __set_fbdc_bw(enum DISP_MODULE_ENUM module,
	unsigned int bandwidth)
{
#ifdef MTK_FB_MMDVFS_SUPPORT
	struct mm_qos_request *request;

	switch (module) {
	case DISP_MODULE_OVL0:
		request = &ovl0_fbdc_request;
		break;
	case DISP_MODULE_OVL0_2L:
		request = &ovl0_2l_fbdc_request;
		break;
	default:
		DISPERR("unsupport module id %s(%d)\n",
			ddp_get_module_name(module), module);
		return -1;
	}
	bandwidth = bandwidth * OCCUPIED_BW_RATIO / 1000;
	mm_qos_set_bw_request(request, bandwidth, BW_COMP_DEFAULT);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_PULSE,
			module, bandwidth);

	return 0;
}

int disp_pm_qos_update_bw(unsigned long long bandwidth)
{
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_START,
			!primary_display_is_decouple_mode(), bandwidth);
#ifdef MTK_FB_MMDVFS_SUPPORT
	//mm_qos_update_all_request(&bw_request_list);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->primary_pm_qos,
			MMPROFILE_FLAG_END,
			!primary_display_is_decouple_mode(), bandwidth);

	return 0;
}

int disp_pm_qos_update_hrt(unsigned long long bandwidth)
{
	mmprofile_log_ex(ddp_mmp_get_events()->primary_hrt_bw,
			MMPROFILE_FLAG_START,
			!primary_display_is_decouple_mode(), bandwidth);
#ifdef MTK_FB_MMDVFS_SUPPORT
	//mm_qos_update_all_request(&hrt_request_list);
#endif
	mmprofile_log_ex(ddp_mmp_get_events()->primary_hrt_bw,
			MMPROFILE_FLAG_END,
			!primary_display_is_decouple_mode(), bandwidth);

	return 0;
}

unsigned int get_has_hrt_bw(void)
{
	return has_hrt_bw;
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
int prim_disp_request_hrt_bw(int overlap_num,
			enum DDP_SCENARIO_ENUM scenario, const char *caller,
			unsigned int active_cfg)
{
	unsigned long long bw_base;
	unsigned int tmp;
	unsigned int wdma_bw;

	/* overlap_num in PAN_DISP ioctl or Assert layer is 0 */
	if (overlap_num == HRT_BW_BYPASS)
		return 0;
	else if (overlap_num == HRT_BW_UNREQ) {
		overlap_num = 0;
		has_hrt_bw = 0;
	} else
		has_hrt_bw = 1;

	bw_base = layering_get_frame_bw(active_cfg);
	bw_base /= 2;

	tmp = bw_base * overlap_num;
	wdma_bw = bw_base * 2;

	switch (scenario) {
	case DDP_SCENARIO_PRIMARY_DISP:
		__set_hrt_bw(DISP_MODULE_OVL0, tmp);
		__set_hrt_bw(DISP_MODULE_OVL0_2L, tmp);
		__set_hrt_bw(DISP_MODULE_WDMA0, 0);
		__set_hrt_bw(DISP_MODULE_RDMA0, 0);
		break;
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
		__set_hrt_bw(DISP_MODULE_OVL0, 0);
		__set_hrt_bw(DISP_MODULE_OVL0_2L, 0);
		__set_hrt_bw(DISP_MODULE_WDMA0, 0);
		__set_hrt_bw(DISP_MODULE_RDMA0, tmp);
		break;
	case DDP_SCENARIO_PRIMARY_ALL:
		__set_hrt_bw(DISP_MODULE_OVL0, tmp);
		__set_hrt_bw(DISP_MODULE_OVL0_2L, tmp);
		__set_hrt_bw(DISP_MODULE_WDMA0, wdma_bw);
		__set_hrt_bw(DISP_MODULE_RDMA0, wdma_bw);
		break;
	default:
		DISPINFO("invalid HRT scenario %s\n",
				ddp_get_scenario_name(scenario));
	}
#ifdef MTK_FB_MMDVFS_SUPPORT
	mm_qos_set_hrt_request(&hrt_bw_request, tmp);

	DISPINFO("%s report HRT BW %u MB overlap %d, %llu/s, scen:%u\n",
		caller, tmp, overlap_num, bw_base, scenario);
	mmprofile_log_ex(ddp_mmp_get_events()->primary_hrt_bw,
			MMPROFILE_FLAG_PULSE,
			overlap_num, tmp);
	//mm_qos_update_all_request(&hrt_request_list);
#endif
	return 0;
}
#endif
int disp_pm_qos_set_default_bw(unsigned long long *bandwidth)
{
	__set_bw(DISP_MODULE_OVL0, 0);
	__set_fbdc_bw(DISP_MODULE_OVL0, 0);
	__set_bw(DISP_MODULE_OVL0_2L, 0);
	__set_fbdc_bw(DISP_MODULE_OVL0_2L, 0);
	__set_bw(DISP_MODULE_WDMA0, 0);
	__set_bw(DISP_MODULE_RDMA0, 0);

	*bandwidth = 0;

	return 0;
}

int disp_pm_qos_set_default_hrt(void)
{
	__set_hrt_bw(DISP_MODULE_OVL0, 0);
	__set_hrt_bw(DISP_MODULE_OVL0_2L, 0);
	__set_hrt_bw(DISP_MODULE_WDMA0, 0);
	__set_hrt_bw(DISP_MODULE_RDMA0, 0);
#ifdef MTK_FB_MMDVFS_SUPPORT
	mm_qos_set_hrt_request(&hrt_bw_request, 0);
#endif

	return 0;
}

int disp_pm_qos_set_ovl_bw(unsigned long long in_fps,
			   unsigned long long out_fps,
			   unsigned long long *bandwidth)
{
	int ret = 0;
	unsigned int is_dc;
	unsigned int ovl0_bw, ovl0_2l_bw, rdma0_bw, wdma0_bw;
	unsigned int ovl0_fbdc_bw, ovl0_2l_fbdc_bw;
	unsigned int ovl0_lstf_bw, ovl0_2l_lstf_bw;

	DISPINFO("%s,fps:[in-%llu,out-%llu]\n",
		__func__, in_fps, out_fps);
	ret |= __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_IS_DC, &is_dc);
	ret |= __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_OVL0_BW, &ovl0_bw);
	ret |= __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_OVL0_2L_BW, &ovl0_2l_bw);
	ret |= __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_RDMA0_BW, &rdma0_bw);
	ret |= __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_WDMA0_BW, &wdma0_bw);

	/*read back ovl last frame bw*/

	/*bw req is in unit of 16bytes, transfer to bytes*/
	ovl0_lstf_bw = ovl0_lstf_bw << 4;
	ovl0_2l_lstf_bw = ovl0_2l_lstf_bw << 4;

	DISPDBG("%s ovl0 last bw:%d, ovl0_2l last bw:%d",
		__func__, ovl0_lstf_bw, ovl0_2l_lstf_bw);

	/* cmdq get slot fail */
	if (ret) {
		DISPERR("DISP CMDQ get slot failed:%d\n", ret);
		disp_pm_qos_set_default_bw(bandwidth);
	} else {
		if (is_dc)
			*bandwidth = ((((unsigned long long)ovl0_bw +
				(unsigned long long)ovl0_fbdc_bw +
				(unsigned long long)ovl0_2l_bw +
				(unsigned long long)ovl0_2l_fbdc_bw +
				(unsigned long long)wdma0_bw) * in_fps) +
				((unsigned long long)rdma0_bw * out_fps))*
				OCCUPIED_BW_RATIO;
		else
			*bandwidth = ((unsigned long long)ovl0_bw +
				(unsigned long long)ovl0_fbdc_bw +
				(unsigned long long)ovl0_2l_bw +
				(unsigned long long)ovl0_2l_fbdc_bw) * out_fps *
				OCCUPIED_BW_RATIO;

		do_div(*bandwidth, 1000 * 1000);

		/* Call __set_bw API to setup estimated data bw */
		if (is_dc) {
			ovl0_bw = ovl0_bw * in_fps / 1000;
			ovl0_fbdc_bw = ovl0_fbdc_bw * in_fps / 1000;
			ovl0_2l_bw = ovl0_2l_bw * in_fps / 1000;
			ovl0_2l_fbdc_bw = ovl0_2l_fbdc_bw * in_fps / 1000;
			wdma0_bw = wdma0_bw * in_fps / 1000;
			rdma0_bw = rdma0_bw * out_fps / 1000;

			__set_bw(DISP_MODULE_OVL0, ovl0_bw);
			__set_fbdc_bw(DISP_MODULE_OVL0, ovl0_fbdc_bw);
			__set_bw(DISP_MODULE_OVL0_2L, ovl0_2l_bw);
			__set_fbdc_bw(DISP_MODULE_OVL0_2L, ovl0_2l_fbdc_bw);
			__set_bw(DISP_MODULE_WDMA0, wdma0_bw);
			__set_bw(DISP_MODULE_RDMA0, rdma0_bw);
		} else {
			ovl0_bw = ovl0_bw * out_fps / 1000;
			ovl0_fbdc_bw = ovl0_fbdc_bw * out_fps / 1000;
			ovl0_2l_bw = ovl0_2l_bw * out_fps / 1000;
			ovl0_2l_fbdc_bw = ovl0_2l_fbdc_bw * out_fps / 1000;
			wdma0_bw = 0;
			rdma0_bw = 0;

			__set_bw(DISP_MODULE_OVL0, ovl0_bw);
			__set_fbdc_bw(DISP_MODULE_OVL0, ovl0_fbdc_bw);
			__set_bw(DISP_MODULE_OVL0_2L, ovl0_2l_bw);
			__set_fbdc_bw(DISP_MODULE_OVL0_2L, ovl0_2l_fbdc_bw);
			__set_bw(DISP_MODULE_WDMA0, wdma0_bw);
			__set_bw(DISP_MODULE_RDMA0, rdma0_bw);
		}

		if (is_dc) {
			DISPDBG(
				"%s ovl0:(%d,%d), ovl0_2l:(%d,%d), wdma0:%d, rdma0:%d ",
				__func__,
				ovl0_bw, ovl0_fbdc_bw,
				ovl0_2l_bw, ovl0_2l_fbdc_bw,
				wdma0_bw, rdma0_bw);
			DISPDBG(
				"in_fps:%llu, out_fps:%llu, bw:%llu\n",
				in_fps, out_fps, *bandwidth);
		} else
			DISPDBG(
				"%s ovl0:(%d,%d), ovl0_2l:(%d,%d), out_fps:%llu, bw:%llu\n",
				__func__,
				ovl0_bw, ovl0_fbdc_bw,
				ovl0_2l_bw, ovl0_2l_fbdc_bw,
				out_fps, *bandwidth);
	}

	return ret;
}

int disp_pm_qos_set_rdma_bw(unsigned long long out_fps,
			    unsigned long long *bandwidth)
{
	int ret = 0;
	unsigned int rdma0_bw;
	unsigned int ovl0_bw, ovl0_2l_bw, wdma0_bw;
	unsigned int ovl0_fbdc_bw, ovl0_2l_fbdc_bw;

	ret = __get_cmdq_slots(DISPSYS_SLOT_BASE,
		DISP_SLOT_RDMA0_BW, &rdma0_bw);

	/* cmdq get slot fail */
	if (ret) {
		DISPERR("DISP CMDQ get slot failed:%d\n", ret);
		disp_pm_qos_set_default_bw(bandwidth);
	} else {
		*bandwidth = (unsigned long long)rdma0_bw *
			out_fps * OCCUPIED_BW_RATIO;

		do_div(*bandwidth, 1000 * 1000);

		/* Call __set_bw API to setup estimated data bw */
		ovl0_bw = 0;
		ovl0_fbdc_bw = 0;
		ovl0_2l_bw = 0;
		ovl0_2l_fbdc_bw = 0;
		wdma0_bw = 0;
		rdma0_bw = rdma0_bw * out_fps / 1000;

		__set_bw(DISP_MODULE_OVL0, ovl0_bw);
		__set_fbdc_bw(DISP_MODULE_OVL0, ovl0_fbdc_bw);
		__set_bw(DISP_MODULE_OVL0_2L, ovl0_2l_bw);
		__set_fbdc_bw(DISP_MODULE_OVL0_2L, ovl0_2l_fbdc_bw);
		__set_bw(DISP_MODULE_WDMA0, wdma0_bw);
		__set_bw(DISP_MODULE_RDMA0, rdma0_bw);

		DISPDBG("%s rdma0:%d, out_fps:%llu, bw:%llu\n",
			__func__, rdma0_bw, out_fps, *bandwidth);
	}

	return ret;
}
#endif
