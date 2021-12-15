/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if IS_ENABLED(BUILD_MMQOS)
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#if defined(USE_MEDIATEK_EMI)
#include <memory/mediatek/emi.h>
#include <memory/mediatek/dramc.h>
#elif defined(USE_MTK_DRAMC)
#include <mtk_dramc.h>
#endif

#include "mmdvfs_pmqos.h"
#include "mmdvfs_plat.h"
#include <mt-plat/aee.h>


#include "smi_pmqos.h"
#include "smi_public.h"

#include <helio-dvfsrc-opp.h>

#define CREATE_TRACE_POINTS
#include "mmdvfs_events.h"

#ifdef MMDVFS_MMP
#include "mmprofile.h"
#endif

#ifdef QOS_BOUND_DETECT
#include "mtk_qos_bound.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt) "[mmqos]" fmt

#ifdef MMDVFS_MMP
struct mmqos_mmp_events_t {
	mmp_event mmqos;
	mmp_event hrt_change;
	mmp_event cam_bw_mismatch;
	mmp_event larb_soft_mode;
	mmp_event larb_bwl;
	mmp_event larb_port;
	mmp_event smi_freq;
};
static struct mmqos_mmp_events_t mmqos_mmp_events;
#endif

enum {
	VIRTUAL_DISP_LARB_ID = SMI_LARB_NUM,
	VIRTUAL_MD_LARB_ID,
	VIRTUAL_CCU_COMMON_ID,
	VIRTUAL_CCU_COMMON2_ID,
	MAX_LARB_COUNT
};

#define PORT_VIRTUAL_DISP SMI_PMQOS_ENC(VIRTUAL_DISP_LARB_ID, 0)
#define PORT_VIRTUAL_MD	 SMI_PMQOS_ENC(VIRTUAL_MD_LARB_ID, 0)
#define PORT_VIRTUAL_CCU_COMMON	 SMI_PMQOS_ENC(VIRTUAL_CCU_COMMON_ID, 0)
#define PORT_VIRTUAL_CCU_COMMON2 SMI_PMQOS_ENC(VIRTUAL_CCU_COMMON2_ID, 0)

static u32 log_level;
enum mmdvfs_log_level {
	log_bw = 0,
	log_smi_freq,
	log_qos_validation,
	log_qoslarb,
};

#define UNINITIALIZED_VALUE (-1)
#define MAX_OSTD_NODE_NAME "max_ostd"
static s32 max_ostd = UNINITIALIZED_VALUE;
#define MAX_OSTD_LARB_NODE_NAME "max_ostd_larb"
#define CAM_LARB_NODE_NAME "cam_larb"
#define COMM_FREQ_NODE_NAME "comm_freq"
static u32 cam_larb_size;
static u32 cam_larb_ids[MAX_LARB_COUNT];

static u32 max_bw_bound;
#define MAX_COMM_NUM (2)

static struct mtk_pm_qos_request mm_bw_request;
static struct mtk_pm_qos_request smi_freq_request[MAX_COMM_NUM];
static DEFINE_MUTEX(bw_mutex);
static s32 total_hrt_bw = UNINITIALIZED_VALUE;
static s32 total_ui_only_hrt_bw = UNINITIALIZED_VALUE;
static BLOCKING_NOTIFIER_HEAD(hrt_bw_throttle_notifier);

/* id is from SMI_LARB_L1ARB */
static void get_comm_port_by_id(u32 id, u32 *comm, u32 *comm_port)
{
	*comm = id >> 16;
	*comm_port = id & 0xffff;
}

static inline u32 get_id_by_comm_port(u32 comm, u32 comm_port)
{
	return ((comm << 16) | (comm_port & 0xffff));
}

static bool larb_soft = true;
static u32 default_bwl = 0x200;
static s32 force_larb_mode = -1;
static s32 comm_port_limit[MAX_COMM_NUM][SMI_COMM_MASTER_NUM] = {};
static s32 comm_port_hrt[MAX_COMM_NUM][SMI_COMM_MASTER_NUM] = {};
static s32 force_comm_bwl[MAX_COMM_NUM][SMI_COMM_MASTER_NUM] = {};
static u32 comm_freq_class[MAX_COMM_NUM] = {};
#ifdef MMDVFS_SKIP_SMI_CONFIG
static bool skip_smi_config = true;
#else
static bool skip_smi_config;
#endif
void mm_qos_update_larb_bwl(u32 larb_update, bool bw_change)
{
	u32 i, larb_bw, comm, comm_port;
	bool larb_soft_mode = larb_soft;
	s32 freq[MAX_COMM_NUM];
	const u32 length = MAX_COMM_NUM * SMI_COMM_MASTER_NUM;

	mutex_lock(&bw_mutex);
	if (unlikely(force_larb_mode >= 0))
		larb_soft_mode = force_larb_mode;

	for (i = 0; i < MAX_COMM_NUM; i++) {
		if (comm_freq_class[i] == 0)
			freq[i] = 0;
		else
			freq[i] = mmdvfs_qos_get_freq(comm_freq_class[i]);
	}

	for (i = 0; i < length; i++) {
		if (!(larb_update & (1 << i)))
			continue;
		comm = i / SMI_COMM_MASTER_NUM;
		if (freq[comm] <= 0)
			continue;
		comm_port = i % SMI_COMM_MASTER_NUM;
		larb_bw = 0;
		if (force_comm_bwl[comm][comm_port] != 0) {
			larb_bw = force_comm_bwl[comm][comm_port];
			if (log_level & 1 << log_bw)
				pr_notice("force comm:%d port:%d bwl:%#x\n",
				comm, comm_port, larb_bw);
		} else if (comm_port_limit[comm][comm_port]) {
			larb_bw = (comm_port_limit[comm][comm_port] << 8)
					/ freq[comm];
			if (log_level & 1 << log_bw)
				pr_notice("comm:%d port:%d bwl:%#x bw:%u\n",
				comm, comm_port, larb_bw,
				comm_port_limit[comm][comm_port]);
		}
		if (larb_bw) {
			smi_bwl_update(get_id_by_comm_port(comm, comm_port),
				larb_bw, (comm_port_hrt[comm][comm_port] > 0) ?
				true : larb_soft_mode, "MMDVFS");
			trace_mmqos__update_larb(comm, comm_port,
				comm_port_limit[comm][comm_port], larb_bw,
				(comm_port_hrt[comm][comm_port] > 0) ?
				true : larb_soft_mode);
#ifdef MMDVFS_MMP
			if (mmdvfs_log_larb_mmp(comm_port, -1))
				mmprofile_log_ex(
					mmqos_mmp_events.larb_bwl,
					MMPROFILE_FLAG_PULSE,
					(comm_port << 28) | larb_bw,
					larb_soft_mode);
#endif
		} else if (bw_change) {
			/* if no bwl_bw, set default bwl with soft-mode */
			smi_bwl_update(get_id_by_comm_port(comm, comm_port),
				default_bwl, true, "MMDVFS");
			trace_mmqos__update_larb(comm, comm_port,
				comm_port_limit[comm][comm_port],
				default_bwl, true);
#ifdef MMDVFS_MMP
			if (mmdvfs_log_larb_mmp(comm_port, -1))
				mmprofile_log_ex(
					mmqos_mmp_events.larb_bwl,
					MMPROFILE_FLAG_PULSE,
					(comm_port << 28) | default_bwl, 2);
#endif
		}
	}
	mutex_unlock(&bw_mutex);
}
EXPORT_SYMBOL_GPL(mm_qos_update_larb_bwl);
#define MAX_LARB_NAME 16

static struct mm_larb_request larb_req[MAX_LARB_COUNT] = {};
#define LARB_NODE_NAME "larb_groups"

#define MAX_CH_COUNT 2
static s32 channel_srt_bw[MAX_COMM_NUM][MAX_CH_COUNT] = {};
static s32 channel_hrt_bw[MAX_COMM_NUM][MAX_CH_COUNT] = {};
static s32 channel_disp_hrt_cnt[MAX_COMM_NUM][MAX_CH_COUNT] = {};

#define MULTIPLY_BW_THRESH_HIGH(value) ((value)*1/2)
#define MULTIPLY_BW_THRESHOLD_LOW(value) ((value)*2/5)
#define MULTIPLY_RATIO(value) ((value)*1000)
#define DIVIDE_RATIO(value) ((value)/1000)
static s32 current_hrt_bw;
static u32 camera_max_bw;
static s32 get_cam_hrt_bw(void)
{
	u32 i;
	s32 result = 0;

	for (i = 0; i < cam_larb_size; i++)
		result += larb_req[cam_larb_ids[i]].total_hrt_data;

	return result;
}

static bool is_camera_larb(u32 master_id)
{
	u32 i;
	bool result = false;

	for (i = 0; i < cam_larb_size; i++) {
		if (SMI_PMQOS_LARB_DEC(master_id) == cam_larb_ids[i]) {
			result = true;
			break;
		}
	}

	return result;
}

static s32 get_total_used_hrt_bw(void)
{
	s32 cam_hrt_bw;
	s32 disp_hrt_bw;
	s32 md_hrt_bw;

	/* HRT Write BW should multiply a weight */
	cam_hrt_bw = dram_write_weight(MULTIPLY_RATIO(get_cam_hrt_bw())/cam_occ_ratio());
	disp_hrt_bw =
		MULTIPLY_RATIO(larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)].total_hrt_data)
		/disp_occ_ratio();
	md_hrt_bw =
		larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD)].total_hrt_data;
	return (cam_hrt_bw + disp_hrt_bw + md_hrt_bw);
}

#if defined(USE_MEDIATEK_EMI)
static s32 get_io_width(void)
{
	s32 io_width;
	s32 ddr_type = mtk_dramc_get_ddr_type();

	if (ddr_type == TYPE_LPDDR4 || ddr_type == TYPE_LPDDR4X
	    || ddr_type == TYPE_LPDDR4P || ddr_type == TYPE_LPDDR5)
		io_width = 2;
	else
		io_width = 4;

	return io_width;
}
#elif defined(USE_MTK_DRAMC)
static s32 get_io_width(void)
{
	s32 io_width;
	s32 ddr_type = get_ddr_type();

	if (ddr_type == TYPE_LPDDR3)
		io_width = 4;
	else if (ddr_type == TYPE_LPDDR4 || ddr_type == TYPE_LPDDR4X)
		io_width = 2;
	else
		io_width = 4;

	return io_width;
}
#endif

#ifdef HRT_MECHANISM
#ifdef SIMULATE_DVFSRC
static s32 bw_threshold_high[DDR_OPP_NUM] = {0};
static s32 bw_threshold_low[DDR_OPP_NUM] = {0};
static struct mtk_pm_qos_request ddr_request;


static void init_simulation(void)
{
	u32 i = 0;

	for (i = 0; i < DDR_OPP_NUM; i++) {
		s32 freq = 0;
#ifdef USE_MTK_DRAMC
		s32 ch_num = get_emi_ch_num();
		/* Todo: Use API from DRAM owner */
		s32 io_width = get_io_width();

		/* Todo: It should be modified in P80 */
		if (i == 0)
			freq = dram_steps_freq(i) * ch_num * io_width;
		else
			freq = dram_steps_freq(i+1) * ch_num * io_width;
#endif
		bw_threshold_high[i] =
			(s32)MULTIPLY_BW_THRESH_HIGH(freq);
		bw_threshold_low[i] =
			(s32)MULTIPLY_BW_THRESHOLD_LOW(freq);
	}

	mtk_pm_qos_add_request(
		&ddr_request, MTK_PM_QOS_DDR_OPP,  PM_QOS_DDR_OPP_DEFAULT_VALUE);
}

static u32 get_ddr_opp_by_threshold(s32 bw, s32 *threshold_array)
{
	s32 i = 0;
	u32 opp = 0;

	/**
	 * From small value to large value.
	 * Find the first threshold which is larger than input bw.
	 * If no threshold is found, it must be highest level of DDR.
	 */
	for (i = DDR_OPP_NUM-1; i >= 0; i--) {
		if (bw < threshold_array[i]) {
			opp = i;
			break;
		}
	}
	return opp;
}

static void simulate_dvfsrc(s32 next_hrt_bw)
{
	u32 current_opp, next_opp;
	s32 *threshold_array;
	bool is_up = false;

	if (next_hrt_bw > current_hrt_bw) {
		threshold_array = &bw_threshold_high[0];
		is_up = true;
	} else
		threshold_array = &bw_threshold_low[0];

	current_opp = get_ddr_opp_by_threshold(current_hrt_bw, threshold_array);
	next_opp = get_ddr_opp_by_threshold(next_hrt_bw, threshold_array);

	if ((is_up && next_opp < current_opp) ||
		(!is_up && next_opp > current_opp)) {
		mtk_pm_qos_update_request(&ddr_request, next_opp);
		if (log_level & 1 << log_bw)
			pr_notice("up=%d copp=%d nopp=%d cbw=%d nbw=%d\n",
				is_up, current_opp, next_opp,
				current_hrt_bw, next_hrt_bw);
	}
}
#else
static struct mtk_pm_qos_request dvfsrc_isp_hrt_req;
static void init_dvfsrc(void)
{
	mtk_pm_qos_add_request(
		&dvfsrc_isp_hrt_req, MTK_PM_QOS_ISP_HRT_BANDWIDTH,
		PM_QOS_ISP_HRT_BANDWIDTH_DEFAULT_VALUE);
}
#endif

static void log_hrt_bw_info(u32 master_id)
{
	s32 ccu_hrt_bw = get_ccu_hrt_bw(larb_req);
	s32 p1_hrt_bw = get_cam_hrt_bw() - ccu_hrt_bw;
	s32 disp_hrt_bw =
		larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)].total_hrt_data;
	u32 ddr_opp = get_cur_ddr_opp();
#ifdef MMDVFS_MMP
	u32 param1 = (SMI_PMQOS_LARB_DEC(master_id) << 24) |
		(ddr_opp << 16) | disp_hrt_bw;
	u32 param2 = (ccu_hrt_bw << 16) | p1_hrt_bw;

	mmprofile_log_ex(
		mmqos_mmp_events.hrt_change,
		MMPROFILE_FLAG_PULSE, param1, param2);
#endif

	if (log_level & 1 << log_bw)
		pr_notice("%s larb=%d p1=%d ccu=%d disp=%d ddr_opp=%d\n",
			__func__, SMI_PMQOS_LARB_DEC(master_id), p1_hrt_bw,
			ccu_hrt_bw, disp_hrt_bw, ddr_opp);
}

static void update_hrt_bw_to_dvfsrc(s32 next_hrt_bw)
{
#ifdef SIMULATE_DVFSRC
	simulate_dvfsrc(next_hrt_bw);
#else
	u32 md_larb_id = SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD);
	s32 mm_used_hrt_bw =
		next_hrt_bw - larb_req[md_larb_id].total_hrt_data;

	mtk_pm_qos_update_request(&dvfsrc_isp_hrt_req, mm_used_hrt_bw);
	if (log_level & 1 << log_bw)
		pr_notice("%s report dvfsrc mm_hrt_bw=%d\n",
			__func__, mm_used_hrt_bw);
#endif
}

#endif /* HRT_MECHANISM */


#ifdef BLOCKING_MECHANISM
static atomic_t lock_cam_count = ATOMIC_INIT(0);
static wait_queue_head_t hrt_wait;
#define WAIT_TIMEOUT_MS 200

static void blocking_camera(void)
{
	u32 wait_result;

	pr_notice("begin to blocking for camera_max_bw=%d\n", camera_max_bw);
	wait_result = wait_event_timeout(
		hrt_wait, atomic_read(&lock_cam_count) == 0,
		msecs_to_jiffies(WAIT_TIMEOUT_MS));
	pr_notice("blocking wait_result=%d\n", wait_result);
}
#endif

static void trace_qos_validation(void)
{
	struct mm_qos_request *req = NULL;
	u16 port_index_list[MAX_PORT_COUNT];
	u32 i, j, port_id;
	s32 bw;

	for (i = 0; i < ARRAY_SIZE(larb_req); i++) {
		if (!larb_req[i].port_count)
			continue;
		for (j = 0; j < MAX_PORT_COUNT; j++)
			port_index_list[j] = 0;
		list_for_each_entry(req, &larb_req[i].larb_list, larb_node) {
			/* Make one trace for each request instead of for each
			 * port because it's hard to calculate data size when
			 * one port with many requests (BW and fps are mixed)
			 */
			port_id = SMI_PMQOS_PORT_MASK(req->master_id);
			port_index_list[port_id]++;
			bw = get_comp_value(req->bw_value,
						req->comp_type, true);
			if (req->updated || bw > 0)
				trace_mmqos__update_qosbw(i, port_id,
					port_index_list[port_id], bw);
		}
	}
}

static inline void init_larb_list(u32 larb_id)
{
	if (!larb_req[larb_id].larb_list_init) {
		INIT_LIST_HEAD(&(larb_req[larb_id].larb_list));
		larb_req[larb_id].larb_list_init = true;
	}
}

s32 mm_qos_add_request(struct plist_head *owner_list,
	struct mm_qos_request *req, u32 smi_master_id)
{
	u32 larb_id, port_id;
	struct mm_qos_request *enum_req = NULL;

	larb_id = SMI_PMQOS_LARB_DEC(smi_master_id);
	port_id = SMI_PMQOS_PORT_MASK(smi_master_id);
	if (!req) {
		pr_notice("mm_add: Invalid req pointer\n");
		return -EINVAL;
	}
	if (larb_id >= MAX_LARB_COUNT || port_id >= MAX_PORT_COUNT) {
		pr_notice("mm_add(0x%08x) Invalid master_id\n", smi_master_id);
		return -EINVAL;
	}
	if (req->init) {
		pr_notice("mm_add(0x%08x) req is init\n", req->master_id);
		return -EINVAL;
	}

	req->master_id = smi_master_id;
	req->bw_value = 0;
	req->hrt_value = 0;
	plist_node_init(&(req->owner_node), smi_master_id);
	plist_add(&(req->owner_node), owner_list);
	INIT_LIST_HEAD(&(req->larb_node));
	INIT_LIST_HEAD(&(req->port_node));
	init_larb_list(larb_id);

	mutex_lock(&bw_mutex);
	list_add_tail(&(req->larb_node), &(larb_req[larb_id].larb_list));
	req->init = true;

	list_for_each_entry(enum_req, &larb_req[larb_id].larb_list, larb_node) {
		if (enum_req != req && req->master_id == enum_req->master_id) {
			list_add_tail(&(req->port_node),
				&(enum_req->port_node));
			break;
		}
	}
	mutex_unlock(&bw_mutex);

	if (log_level & 1 << log_bw) {
		pr_notice("mm_add larb=%u port=%d\n", larb_id, port_id);
		pr_notice("req=%p\n", req);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mm_qos_add_request);

#define SHIFT_ROUND(a, b) ((((a) - 1) >> (b)) + 1)
s32 mm_qos_set_request(struct mm_qos_request *req, u32 bw_value,
	u32 hrt_value, u32 comp_type)
{
	u32 larb, port, bw, old_larb_mix_value;
	u32 old_comp_bw, old_comp_limit, new_comp_bw, new_comp_limit;
	u32 comm, comm_port;
	struct mm_qos_request *enum_req = NULL;
	bool hrt_port = false;

	if (!req)
		return -EINVAL;

	larb = SMI_PMQOS_LARB_DEC(req->master_id);
	port = SMI_PMQOS_PORT_MASK(req->master_id);
	if (!req->init || larb >= MAX_LARB_COUNT ||
		port >= MAX_PORT_COUNT || comp_type >= BW_COMP_END) {
		pr_notice("mm_set(0x%08x) init=%d larb=%d port=%d comp=%d\n",
			req->master_id, req->init, larb, port, comp_type);
		dump_stack();
		return -EINVAL;
	}
	if (!larb_req[larb].port_count || !larb_req[larb].ratio[port]) {
		pr_notice("mm_set(0x%08x) invalid port_cnt=%d ratio=%d\n",
			req->master_id, larb_req[larb].port_count,
			larb_req[larb].ratio[port]);
		return -EINVAL;
	}

	if (bw_value > max_bw_bound || hrt_value > max_bw_bound) {
		pr_notice("mm_set(0x%08x) invalid bw=%d hrt=%d bw_bound=%d\n",
			req->master_id, bw_value,
			hrt_value, max_bw_bound);
		return -EINVAL;
	}

	if (req->hrt_value == hrt_value &&
		req->bw_value == bw_value &&
		req->comp_type == comp_type) {
		if (log_level & 1 << log_bw)
			pr_notice("mm_set(0x%08x) no change\n", req->master_id);
		return 0;
	}

	mutex_lock(&bw_mutex);

	req->updated = true;
	old_comp_bw = get_comp_value(req->bw_value, req->comp_type, true);
	old_comp_limit = get_comp_value(req->bw_value, req->comp_type, false);
	new_comp_bw = get_comp_value(bw_value, comp_type, true);
	new_comp_limit = get_comp_value(bw_value, comp_type, false);
	/* Update Total QoS BW */
	larb_req[larb].total_bw_data -= old_comp_bw;
	larb_req[larb].total_bw_data += new_comp_bw;

	old_larb_mix_value = larb_req[larb].total_mix_limit;
	get_comm_port_by_id(larb_req[larb].comm_port, &comm, &comm_port);
	if (req->hrt_value) {
		larb_req[larb].total_hrt_data -= req->hrt_value;
		larb_req[larb].total_mix_limit -= req->hrt_value;
		if (larb < MAX_LARB_COUNT &&
			comm_port < SMI_COMM_MASTER_NUM)
			comm_port_hrt[comm][comm_port] -=
				req->hrt_value;
		if (larb < MAX_LARB_COUNT &&
			larb_req[larb].channel < MAX_CH_COUNT) {
			if (larb_req[larb].is_max_ostd)
				channel_disp_hrt_cnt[comm][larb_req[
					larb].channel]--;
			else
				channel_hrt_bw[comm][larb_req[
					larb].channel] -= req->hrt_value;
		}
	} else
		larb_req[larb].total_mix_limit -= old_comp_limit;

	if (hrt_value) {
		larb_req[larb].total_hrt_data += hrt_value;
		larb_req[larb].total_mix_limit += hrt_value;
		if (larb < MAX_LARB_COUNT &&
			comm_port < SMI_COMM_MASTER_NUM)
			comm_port_hrt[comm][comm_port] += hrt_value;
		if (larb < MAX_LARB_COUNT &&
			larb_req[larb].channel < MAX_CH_COUNT) {
			if (larb_req[larb].is_max_ostd)
				channel_disp_hrt_cnt[comm][larb_req[
					larb].channel]++;
			else
				channel_hrt_bw[comm][larb_req[
					larb].channel] += hrt_value;
		}
	} else
		larb_req[larb].total_mix_limit += new_comp_limit;

	if (larb < MAX_LARB_COUNT && larb_req[larb].channel < MAX_CH_COUNT) {
		channel_srt_bw[comm][larb_req[larb].channel] -= old_comp_bw;
		channel_srt_bw[comm][larb_req[larb].channel] += new_comp_bw;
	}

	if (larb < MAX_LARB_COUNT &&
		comm_port < SMI_COMM_MASTER_NUM) {
		comm_port_limit[comm][comm_port] -= old_larb_mix_value;
		comm_port_limit[comm][comm_port] +=
			larb_req[larb].total_mix_limit;
	}

	if (log_level & 1 << log_bw) {
		pr_notice("set=0x%08x comp=%u,%u\n", req->master_id,
		comp_type, req->comp_type);
		pr_notice("set=0x%08x bw=%u,%u total_bw=%d\n", req->master_id,
		bw_value, req->bw_value, larb_req[larb].total_bw_data);
		pr_notice("set=0x%08x hrt=%u,%u total_hrt=%d\n", req->master_id,
		hrt_value, req->hrt_value, larb_req[larb].total_hrt_data);
		pr_notice("set=0x%08x o_mix=%u total_mix=%d\n", req->master_id,
		old_larb_mix_value, larb_req[larb].total_mix_limit);
	}

	req->hrt_value = hrt_value;
	req->bw_value = bw_value;
	req->comp_type = comp_type;

	bw = hrt_value ? SHIFT_ROUND(hrt_value * 3, 1) : new_comp_limit;
	hrt_port = hrt_value;
	list_for_each_entry(enum_req, &(req->port_node), port_node) {
		if (enum_req->hrt_value) {
			bw += enum_req->hrt_value;
			hrt_port = true;
		} else
			bw += get_comp_value(enum_req->bw_value,
				enum_req->comp_type, false);
	}

	req->ostd = bw ? SHIFT_ROUND(bw, larb_req[larb].ratio[port]) : 1;
	if (hrt_port) {
		req->ostd = SHIFT_ROUND(req->ostd * 3, 1);
		if (larb_req[larb].is_max_ostd)
			req->ostd = max_ostd;
	}

	list_for_each_entry(enum_req, &(req->port_node), port_node)
		enum_req->ostd = req->ostd;

	if (log_level & 1 << log_bw)
		pr_notice("mm_set=0x%08x bw=%u ostd=%u hrt=%u comp=%u\n",
			req->master_id, req->bw_value, req->ostd,
			req->hrt_value, req->comp_type);

	mutex_unlock(&bw_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(mm_qos_set_request);

s32 mm_qos_set_bw_request(struct mm_qos_request *req,
	u32 bw_value, s32 comp_type)
{
	return mm_qos_set_request(req, bw_value, req->hrt_value, comp_type);
}
EXPORT_SYMBOL_GPL(mm_qos_set_bw_request);

s32 mm_qos_set_hrt_request(struct mm_qos_request *req,
	u32 hrt_value)
{
	return mm_qos_set_request(req, req->bw_value, hrt_value, 0);
}
EXPORT_SYMBOL_GPL(mm_qos_set_hrt_request);

static u64 cam_scen_start_time;
static bool cam_scen_change;
void mm_qos_update_all_request(struct plist_head *owner_list)
{
	struct mm_qos_request *req = NULL;
	u64 profile;
	u32 i = 0, larb_update = 0, mm_bw = 0;
	s32 next_hrt_bw;
	s32 cam_bw, larb_bw;
	u32 larb_count = 0, larb_id = 0, larb_port_id = 0, larb_port_bw = 0;
	u32 port_id = 0;
	u32 comm, comm_port;
	s32 smi_srt_clk = 0, smi_hrt_clk = 0;
	s32 max_ch_srt_bw = 0, max_ch_hrt_bw = 0;
	s32 final_chn_hrt_bw[MAX_COMM_NUM][MAX_CH_COUNT];
#ifdef CHECK_OSTD_UPDATE
	bool update_ostd;
	struct mm_qos_request *enum_req = NULL;
#endif

	if (!owner_list || plist_head_empty(owner_list)) {
		pr_notice("%s: owner_list is invalid\n", __func__);
		return;
	}

	req = plist_first_entry(owner_list, struct mm_qos_request, owner_node);

	if (is_camera_larb(req->master_id)) {
		cam_bw = dram_write_weight(get_cam_hrt_bw());
		if (cam_bw > camera_max_bw) {
			pr_notice("cam_bw(%d) > camera_max_bw(%d)\n",
				cam_bw, camera_max_bw);
#ifdef MMDVFS_MMP
			mmprofile_log_ex(
				mmqos_mmp_events.cam_bw_mismatch,
				MMPROFILE_FLAG_PULSE,
				cam_bw, camera_max_bw);
#endif
#ifdef AEE_CAM_BW_MISMATCH
			aee_kernel_warning("mmdvfs",
				"cam_bw(%d) > camera_max_bw(%d)\n",
				cam_bw, camera_max_bw);
#endif
		}
		if (cam_scen_change) {
			pr_notice("scenario change time=%u cam_bw=%d\n",
				jiffies_to_msecs(jiffies-cam_scen_start_time),
				cam_bw);
			cam_scen_change = false;
		}
#ifdef BLOCKING_MECHANISM
		if (atomic_read(&lock_cam_count) > 0)
			blocking_camera();
#endif
		if (total_hrt_bw != UNINITIALIZED_VALUE &&
			get_total_used_hrt_bw() > total_hrt_bw)
			pr_notice("hrt bw overflow used=%d avail=%d\n",
				get_total_used_hrt_bw(), total_hrt_bw);
	}

	mutex_lock(&bw_mutex);
	next_hrt_bw = get_total_used_hrt_bw();
	if (next_hrt_bw != current_hrt_bw) {
#ifdef HRT_MECHANISM
		update_hrt_bw_to_dvfsrc(next_hrt_bw);
		log_hrt_bw_info(req->master_id);
#endif
		current_hrt_bw = next_hrt_bw;
	}
	mutex_unlock(&bw_mutex);

	if (log_level & 1 << log_qos_validation)
		trace_qos_validation();

	plist_for_each_entry(req, owner_list, owner_node) {
		if (!req->updated)
			continue;
		i++;
		larb_id = SMI_PMQOS_LARB_DEC(req->master_id);
		port_id = SMI_PMQOS_PORT_MASK(req->master_id);
		get_comm_port_by_id(larb_req[larb_id].comm_port,
			&comm, &comm_port);
		larb_update |= 1 << (comm * SMI_COMM_MASTER_NUM + comm_port);
		if (log_level & 1 << log_bw)
			pr_notice("update(0x%08x) ostd=%d value=%d hrt=%d\n",
				req->master_id, req->ostd,
				req->bw_value, req->hrt_value);
		trace_mmqos__update_port(larb_id, port_id,
			req->bw_value, req->ostd);
		if (larb_port_id && larb_count == 4) {
#ifdef MMDVFS_MMP
			mmprofile_log_ex(mmqos_mmp_events.larb_port,
				MMPROFILE_FLAG_PULSE,
				larb_port_id, larb_port_bw);
#endif
			larb_count = larb_port_bw = larb_port_id = 0;
		}
		if (mmdvfs_log_larb_mmp(-1, larb_id)) {
			larb_port_bw |= req->ostd << (8 * larb_count);
			larb_port_id |= port_id << (8 * larb_count);
			larb_count++;
		}
#ifdef CHECK_OSTD_UPDATE
		mutex_lock(&bw_mutex);
		if (!req->bw_value && !req->hrt_value) {
			update_ostd = false;
			list_for_each_entry(enum_req,
					&(req->port_node), port_node) {
				if (enum_req->bw_value ||
					enum_req->hrt_value) {
					update_ostd = true;
					break;
				}
			}
			req->updated = update_ostd;
		}
		mutex_unlock(&bw_mutex);
#endif
	}
#ifdef MMDVFS_MMP
	if (larb_count)
		mmprofile_log_ex(
			mmqos_mmp_events.larb_port,
			MMPROFILE_FLAG_PULSE, larb_port_id, larb_port_bw);
#endif
	if (!skip_smi_config) {
		profile = sched_clock();
		smi_ostd_update(owner_list, "MMDVFS");
		if (log_level & 1 << log_bw)
			pr_notice("config SMI (%d) cost: %llu us\n",
				i, div_u64(sched_clock() - profile, 1000));
	}

	/* update SMI clock */
	for (comm = 0; comm < MAX_COMM_NUM; comm++) {
		if (comm_freq_class[comm] == 0)
			continue;
		max_ch_srt_bw = 0;
		max_ch_hrt_bw = 0;
		for (i = 0; i < MAX_CH_COUNT; i++) {
			/* channel_hrt_bw[] doesn't contain disp HRT BW, so
			 * add one HRT BW to it if disp HRT count > 0
			 */
			final_chn_hrt_bw[comm][i] =
				channel_disp_hrt_cnt[comm][i] > 0 ?
				channel_hrt_bw[comm][i] +
					larb_req[SMI_PMQOS_LARB_DEC(
					PORT_VIRTUAL_DISP)].total_hrt_data :
				channel_hrt_bw[comm][i];
			max_ch_srt_bw = max_t(s32,
				channel_srt_bw[comm][i], max_ch_srt_bw);
			max_ch_hrt_bw = max_t(s32,
				final_chn_hrt_bw[comm][i], max_ch_hrt_bw);
			if (log_level & 1 << log_smi_freq)
				pr_notice("comm:%d chn:%d s_bw:%d h_bw:%d\n",
					comm, i, channel_srt_bw[comm][i],
					final_chn_hrt_bw[comm][i]);
#ifdef MMDVFS_MMP
			mmprofile_log_ex(
				mmqos_mmp_events.smi_freq,
				MMPROFILE_FLAG_PULSE,
				((comm+1) << 28) | (i << 24) | min_t(s32,
					channel_srt_bw[comm][i], 0xffff),
				((comm+1) << 28) | (i << 24) | min_t(s32,
					final_chn_hrt_bw[comm][i], 0xffff));
#endif
		}
		smi_srt_clk = max_ch_srt_bw ?
			SHIFT_ROUND(max_ch_srt_bw, 4) : 0;
		smi_hrt_clk = max_ch_hrt_bw ?
			SHIFT_ROUND(max_ch_hrt_bw, 4) : 0;
		mtk_pm_qos_update_request(&smi_freq_request[comm],
			max_t(s32, smi_srt_clk, smi_hrt_clk));
		if (log_level & 1 << log_smi_freq)
			pr_notice("comm:%d smi_srt_clk:%d smi_hrt_clk:%d\n",
				comm, smi_srt_clk, smi_hrt_clk);
#ifdef MMDVFS_MMP
		mmprofile_log_ex(
			mmqos_mmp_events.smi_freq,
			MMPROFILE_FLAG_PULSE,
			comm, (min_t(s32, smi_srt_clk, 0xffff) << 16) |
			min_t(s32, smi_hrt_clk, 0xffff));
#endif
	}

	/* update larb-level BW */
	if (!skip_smi_config)
		mm_qos_update_larb_bwl(larb_update, true);

#ifdef QOS_BOUND_DETECT
	mmdvfs_update_qos_sram(larb_req, larb_update);
#endif

	/* update mm total bw */
	for (i = 0; i < MAX_LARB_COUNT; i++) {
		larb_bw = (larb_req[i].comm_port != SMI_COMM_MASTER_NUM) ?
			larb_req[i].total_bw_data : 0;
		mm_bw += larb_bw;
		if (log_level & 1 << log_qoslarb)
			trace_mmqos__update_qoslarb(i, larb_bw);
	}
	mtk_pm_qos_update_request(&mm_bw_request, mm_bw);
	if (log_level & 1 << log_bw)
		pr_notice("config mm_bw=%d\n", mm_bw);
}
EXPORT_SYMBOL_GPL(mm_qos_update_all_request);

void mm_qos_update_all_request_zero(struct plist_head *owner_list)
{
	struct mm_qos_request *req = NULL;

	plist_for_each_entry(req, owner_list, owner_node) {
		mm_qos_set_request(req, 0, 0, 0);
	}
	mm_qos_update_all_request(owner_list);
}
EXPORT_SYMBOL_GPL(mm_qos_update_all_request_zero);

void mm_qos_remove_all_request(struct plist_head *owner_list)
{
	struct mm_qos_request *temp, *req = NULL;

	mutex_lock(&bw_mutex);
	plist_for_each_entry_safe(req, temp, owner_list, owner_node) {
		pr_notice("mm_del(0x%08x)\n", req->master_id);
		plist_del(&(req->owner_node), owner_list);
		list_del(&(req->larb_node));
		list_del(&(req->port_node));
		req->init = false;
	}
	mutex_unlock(&bw_mutex);
}
EXPORT_SYMBOL_GPL(mm_qos_remove_all_request);

static s32 disp_bw_ceiling;
static bool wait_next_max_cam_bw_set;
s32 mm_hrt_get_available_hrt_bw(u32 master_id)
{
	s32 total_used_hrt_bw = get_total_used_hrt_bw();
	s32 src_hrt_bw = larb_req[SMI_PMQOS_LARB_DEC(master_id)].total_hrt_data;
	s32 cam_occ_bw;
	s32 cam_occ_max_bw;
	s32 result;

	if (total_hrt_bw == UNINITIALIZED_VALUE)
		return UNINITIALIZED_VALUE;
	if (total_ui_only_hrt_bw == UNINITIALIZED_VALUE)
		return UNINITIALIZED_VALUE;

	cam_occ_bw = dram_write_weight(MULTIPLY_RATIO(get_cam_hrt_bw())/cam_occ_ratio());
	if (is_camera_larb(master_id))
		src_hrt_bw = cam_occ_bw;
	else
		src_hrt_bw = MULTIPLY_RATIO(src_hrt_bw)/disp_occ_ratio();

	if (camera_max_bw > 0)
		result = total_hrt_bw - total_used_hrt_bw + src_hrt_bw;
	else
		result = total_ui_only_hrt_bw - total_used_hrt_bw + src_hrt_bw;

	if (SMI_PMQOS_LARB_DEC(master_id) ==
			SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)) {
		/* Consider worst camera bw if camera is on */
		cam_occ_max_bw = MULTIPLY_RATIO(camera_max_bw)/cam_occ_ratio();
		if (cam_occ_max_bw > 0)
			result = result + cam_occ_bw - cam_occ_max_bw;

		if (disp_bw_ceiling > 0 && !wait_next_max_cam_bw_set
			&& disp_bw_ceiling < result)
			result = disp_bw_ceiling;
	}

	if (is_camera_larb(master_id))
		result = DIVIDE_RATIO(result * cam_occ_ratio());
	else
		result = DIVIDE_RATIO(result * disp_occ_ratio());

	return ((result < 0)?0:result);
}
EXPORT_SYMBOL_GPL(mm_hrt_get_available_hrt_bw);

s32 mm_hrt_add_bw_throttle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
				&hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mm_hrt_add_bw_throttle_notifier);

s32 mm_hrt_remove_bw_throttle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
				&hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mm_hrt_remove_bw_throttle_notifier);

#ifdef HRT_MECHANISM
static int notify_bw_throttle(void *data)
{
	u64 start_jiffies = jiffies;

	blocking_notifier_call_chain(&hrt_bw_throttle_notifier,
		(camera_max_bw > 0)?BW_THROTTLE_START:BW_THROTTLE_END, NULL);

	pr_notice("notify_time=%u\n",
		jiffies_to_msecs(jiffies-start_jiffies));
	return 0;
}

#ifdef BLOCKING_MECHANISM
static int notify_bw_throttle_blocking(void *data)
{
	notify_bw_throttle(data);

	atomic_dec(&lock_cam_count);
	wake_up(&hrt_wait);
	pr_notice("decrease lock_cam_count=%d\n",
		atomic_read(&lock_cam_count));
	return 0;
}
#endif

static u32 camera_overlap_bw;
static void set_camera_max_bw(u32 occ_bw)
{
	struct task_struct *pKThread;

	camera_max_bw = occ_bw;
	wait_next_max_cam_bw_set = false;
	pr_notice("set cam max occupy_bw=%d\n", occ_bw);
#ifdef BLOCKING_MECHANISM
	/* No need to blocking if cam bw is decreasing */
	if (camera_overlap_bw == 0) {
		atomic_inc(&lock_cam_count);
		pr_notice("increase lock_cam_count=%d\n",
			atomic_read(&lock_cam_count));
		pKThread = kthread_run(notify_bw_throttle_blocking,
			NULL, "notify bw throttle blocking");
		return;
	}
#endif
	pKThread = kthread_run(notify_bw_throttle,
		NULL, "notify bw throttle");
}

static void delay_work_handler(struct work_struct *work)
{
	set_camera_max_bw(camera_overlap_bw);
}
static DECLARE_DELAYED_WORK(g_delay_work, delay_work_handler);
#endif /* HRT_MECHANISM */

void mmdvfs_set_max_camera_hrt_bw(u32 bw)
{
#ifdef HRT_MECHANISM
	u32 mw_hrt_bw;

	cam_scen_change = true;
	cam_scen_start_time = jiffies;

	cancel_delayed_work_sync(&g_delay_work);

	mw_hrt_bw = dram_write_weight(bw);
	if (mw_hrt_bw < camera_max_bw) {
		camera_overlap_bw = mw_hrt_bw;
		schedule_delayed_work(&g_delay_work, 2 * HZ);
	} else {
		camera_overlap_bw = 0;
		set_camera_max_bw(mw_hrt_bw);
	}

	pr_notice("middleware set max camera hrt bw:%d\n", bw);
#endif
//mm_hrt_get_available_hrt_bw(get_virtual_port(VIRTUAL_DISP));
}
EXPORT_SYMBOL_GPL(mmdvfs_set_max_camera_hrt_bw);

static s32 get_total_hrt_bw(bool ui_only)
{
	s32 result = 0;
#if defined(USE_MEDIATEK_EMI)
	s32 max_freq = get_opp_ddr_freq(0)/1000;
	s32 ch_num = mtk_emicen_get_ch_cnt();
	s32 io_width = get_io_width();

	if (ui_only)
		result = DIVIDE_RATIO(max_freq * ch_num * io_width * emi_occ_ui_only());
	else
		result = DIVIDE_RATIO(max_freq * ch_num * io_width * emi_occ_ratio());
#elif defined(USE_MTK_DRAMC)
	s32 max_freq = dram_steps_freq(0);
	s32 ch_num = get_emi_ch_num();
	s32 io_width = get_io_width();

	result = MULTIPLY_BW_THRESH_HIGH(max_freq * ch_num * io_width);
#else
	result = UNINITIALIZED_VALUE;
#endif
	return result;
}

static void mmdvfs_get_larb_node(struct device *dev, u32 larb_id)
{
	u32 value, count = 0;
	const __be32 *p;
	struct property *prop;
	char larb_name[MAX_LARB_NAME];
	s32 result;

	if (larb_id >= MAX_LARB_COUNT) {
		pr_notice("larb_id:%d is over MAX_LARB_COUNT:%d\n",
			larb_id, MAX_LARB_COUNT);
		return;
	}

	result = snprintf(larb_name, MAX_LARB_NAME, "larb%d", larb_id);
	if (result < 0)
		pr_notice("snprintf fail(%d) larb_id=%d\n", result, larb_id);
	of_property_for_each_u32(dev->of_node, larb_name, prop, p, value) {
		if (count >= MAX_PORT_COUNT) {
			pr_notice("port size is over (%d)\n", MAX_PORT_COUNT);
			break;
		}

		larb_req[larb_id].ratio[count] = value;
		count++;
	}

	larb_req[larb_id].port_count = count;
	if (!count)
		pr_notice("no data in larb (%s)\n", larb_name);
	else
		init_larb_list(larb_id);
}

static void init_virtual_larbs(void)
{
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)].port_count = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)].ratio[0] = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP)].channel = MAX_CH_COUNT;
	larb_req[SMI_PMQOS_LARB_DEC(
		PORT_VIRTUAL_DISP)].comm_port = SMI_COMM_MASTER_NUM;
	init_larb_list(SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_DISP));

	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON)].port_count = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON)].ratio[0] = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON)].channel =
		SMI_COMM_BUS_SEL[mmdvfs_get_ccu_smi_common_port(
		PORT_VIRTUAL_CCU_COMMON) & 0xffff];
	larb_req[SMI_PMQOS_LARB_DEC(
		PORT_VIRTUAL_CCU_COMMON)].comm_port =
		mmdvfs_get_ccu_smi_common_port(PORT_VIRTUAL_CCU_COMMON);
	init_larb_list(SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON));

	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON2)].port_count = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON2)].ratio[0] = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON2)].channel =
		SMI_COMM_BUS_SEL[mmdvfs_get_ccu_smi_common_port(
		PORT_VIRTUAL_CCU_COMMON2) & 0xffff];
	larb_req[SMI_PMQOS_LARB_DEC(
		PORT_VIRTUAL_CCU_COMMON2)].comm_port =
		mmdvfs_get_ccu_smi_common_port(PORT_VIRTUAL_CCU_COMMON2);
	init_larb_list(SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_CCU_COMMON2));

	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD)].port_count = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD)].ratio[0] = 1;
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD)].channel = MAX_CH_COUNT;
	larb_req[SMI_PMQOS_LARB_DEC(
		PORT_VIRTUAL_MD)].comm_port = SMI_COMM_MASTER_NUM;
	init_larb_list(SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD));
	larb_req[SMI_PMQOS_LARB_DEC(PORT_VIRTUAL_MD)].total_hrt_data =
						get_md_hrt_bw();
}

static int mmqos_probe(struct platform_device *pdev)
{
	u32 i, value, comm_count = 0;
	struct device_node *node = pdev->dev.of_node;
	struct property *prop;
	const __be32 *p;

#ifdef MMDVFS_MMP
	mmprofile_enable(1);
	if (mmqos_mmp_events.mmqos == 0) {
		mmqos_mmp_events.mmqos =
			mmprofile_register_event(MMP_ROOT_EVENT, "MMQOS");
		mmqos_mmp_events.hrt_change = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "hrt_change");
		mmqos_mmp_events.cam_bw_mismatch = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "cam_bw_mismatch");
		mmqos_mmp_events.larb_soft_mode = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "larb_soft_mode");
		mmqos_mmp_events.larb_bwl = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "larb_bwl");
		mmqos_mmp_events.larb_port = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "larb_port");
		mmqos_mmp_events.smi_freq = mmprofile_register_event(
			mmqos_mmp_events.mmqos, "smi_freq");
		mmprofile_enable_event_recursive(mmqos_mmp_events.mmqos, 1);
	}
	mmprofile_start(1);
#endif

	mtk_pm_qos_add_request(&mm_bw_request, MTK_PM_QOS_MEMORY_BANDWIDTH,
		PM_QOS_MM_MEMORY_BANDWIDTH_DEFAULT_VALUE);

	of_property_for_each_u32(
		node, COMM_FREQ_NODE_NAME, prop, p, value) {
		if (value == 0)
			comm_freq_class[comm_count] = PM_QOS_DISP_FREQ;
		else if (value == 1)
			comm_freq_class[comm_count] = PM_QOS_MDP_FREQ;
		else
			pr_notice("[mmqos]wrong comm_freq value:%d\n", value);
		mtk_pm_qos_add_request(&smi_freq_request[comm_count],
			comm_freq_class[comm_count],
			PM_QOS_MM_FREQ_DEFAULT_VALUE);
		comm_count++;
	}

	cam_larb_size = 0;
	of_property_for_each_u32(node, CAM_LARB_NODE_NAME, prop, p, value) {
		if (cam_larb_size >= MAX_LARB_COUNT) {
			pr_notice(
				"cam_larb is over the MAX_LARB_COUNT (%d)\n",
				MAX_LARB_COUNT);
			break;
		}
		cam_larb_ids[cam_larb_size] = value;
		cam_larb_size++;
	}

	of_property_for_each_u32(
		node, MAX_OSTD_LARB_NODE_NAME, prop, p, value) {
		if (value >= MAX_LARB_COUNT) {
			pr_notice(
				"max_ostd_larb (%d) is over the MAX_LARB_COUNT (%d)\n",
				value, MAX_LARB_COUNT);
			continue;
		}
		larb_req[value].is_max_ostd = true;
	}
	of_property_read_s32(node, MAX_OSTD_NODE_NAME, &max_ostd);
	if (max_ostd != UNINITIALIZED_VALUE)
		max_bw_bound = max_ostd * 256 * 2; /* 256:Write BW, 2: HRT */

	of_property_for_each_u32(node, LARB_NODE_NAME, prop, p, value) {
		mmdvfs_get_larb_node(&pdev->dev, value);
	}

#ifdef HRT_MECHANISM
#ifdef SIMULATE_DVFSRC
	init_simulation();
#else
	init_dvfsrc();
#endif
#endif

	if (SMI_LARB_NUM != 0)
		init_virtual_larbs();

	for (i = 0; i < SMI_LARB_NUM; i++) {
		value = SMI_LARB_L1ARB[i];
		larb_req[i].comm_port = value;
		if (value != SMI_COMM_MASTER_NUM)
			larb_req[i].channel =
				SMI_COMM_BUS_SEL[value & 0xffff];
		pr_notice("larb[%d].comm_port=%d channel=%d\n",
				i, value, larb_req[i].channel);
	}

#ifdef BLOCKING_MECHANISM
	init_waitqueue_head(&hrt_wait);
#endif
	return 0;

}

static int mmqos_remove(struct platform_device *pdev)
{
	u32 i;

	mtk_pm_qos_remove_request(&mm_bw_request);
	for (i = 0; i < MAX_COMM_NUM; i++) {
		if (comm_freq_class[i] == 0)
			continue;
		mtk_pm_qos_remove_request(&smi_freq_request[i]);
	}

#ifdef HRT_MECHANISM
#ifdef SIMULATE_DVFSRC
	mtk_pm_qos_remove_request(&ddr_request);
#else
	mtk_pm_qos_remove_request(&dvfsrc_isp_hrt_req);
#endif
#endif
	return 0;
}

static const struct of_device_id mmqos_of_ids[] = {
	{.compatible = "mediatek,mmqos",},
	{}
};

static struct platform_driver mmqos_driver = {
	.probe = mmqos_probe,
	.remove = mmqos_remove,
	.driver = {
		   .name = "mtk_mmqos",
		   .owner = THIS_MODULE,
		   .of_match_table = mmqos_of_ids,
	}
};

static int __init mmqos_init(void)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
	return 0;
#else
	s32 status;

	status = platform_driver_register(&mmqos_driver);
	if (status != 0) {
		pr_notice(
			"Failed to register MMQOS driver(%d)\n", status);
		return -ENODEV;
	}

	pr_notice("%s\n", __func__);
	return 0;
#endif /* CONFIG_FPGA_EARLY_PORTING */
}

#ifdef QOS_BOUND_DETECT
static int system_qos_update(struct notifier_block *nb,
		unsigned long qos_status, void *v)
{
	larb_soft = !(qos_status > QOS_BOUND_BW_FREE);
#ifdef MMDVFS_MMP
	mmprofile_log_ex(
		mmqos_mmp_events.larb_soft_mode,
		MMPROFILE_FLAG_PULSE, larb_soft, qos_status);
#endif
	if (likely(force_larb_mode < 0) && !skip_smi_config)
		mm_qos_update_larb_bwl(0xFFFF, false);

	return NOTIFY_OK;
}

struct system_qos_status {
	struct notifier_block nb;
};

static struct system_qos_status system_qos = {
	.nb.notifier_call = system_qos_update,
};
#endif

static void __exit mmqos_exit(void)
{
	platform_driver_unregister(&mmqos_driver);
#ifdef QOS_BOUND_DETECT
	unregister_qos_notifier(&system_qos.nb);
#endif
}

static int __init mmqos_late_init(void)
{
#ifdef QOS_BOUND_DETECT
	register_qos_notifier(&system_qos.nb);
#endif
	total_hrt_bw = get_total_hrt_bw(false);
	total_ui_only_hrt_bw = get_total_hrt_bw(true);
	return 0;
}

s32 get_virtual_port(enum virtual_source_id id)
{
	switch (id) {
	case VIRTUAL_DISP:
		return PORT_VIRTUAL_DISP;
	case VIRTUAL_MD:
		return PORT_VIRTUAL_MD;
	case VIRTUAL_CCU_COMMON:
		return PORT_VIRTUAL_CCU_COMMON;
	case VIRTUAL_CCU_COMMON2:
		return PORT_VIRTUAL_CCU_COMMON2;
	default:
		pr_notice("invalid source id:%u\n", id);
		return -1;
	}
}

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "mmqos log level");

module_param(skip_smi_config, bool, 0644);
MODULE_PARM_DESC(skip_smi_config, "mmqos smi config");

static u32 dump_larbs = 0xFFFFFFFF;
#define MAX_DUMP (PAGE_SIZE - 1)
int get_larbs_info(char *buf)
{
	u32 i, j;
	int length = 0;
	struct mm_qos_request *req = NULL;

	for (i = 0; i < ARRAY_SIZE(larb_req); i++) {
		if (!larb_req[i].port_count || !(dump_larbs & 1 << i))
			continue;
		length += snprintf(buf + length, MAX_DUMP - length,
			"[%u] port count: %u\n", i, larb_req[i].port_count);
		for (j = 0; j < ARRAY_SIZE(larb_req[i].ratio); j++) {
			if (!larb_req[i].ratio[j])
				break;
			length += snprintf(buf + length, MAX_DUMP - length,
				"  %u", larb_req[i].ratio[j]);
			if (length >= MAX_DUMP)
				break;
		}
		length += snprintf(buf + length, MAX_DUMP - length, "\n");

		mutex_lock(&bw_mutex);
		list_for_each_entry(req, &larb_req[i].larb_list, larb_node) {
			if (!req->bw_value && !req->hrt_value)
				continue;
			length += snprintf(buf + length, MAX_DUMP - length,
				"  [port-%u]: bw=%u ostd=%u hrt=%u comp=%d\n",
				req->master_id & 0x1F, req->bw_value, req->ostd,
				req->hrt_value, req->comp_type);
			if (length >= MAX_DUMP)
				break;
		}
		mutex_unlock(&bw_mutex);

		if (length >= MAX_DUMP)
			break;
	}
	if (length >= MAX_DUMP)
		length = MAX_DUMP - 1;

	return length;
}

void mmdvfs_print_larbs_info(void)
{
	int len;
	char *ptr, *tmp_str;
	char *log_str = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (log_str) {
		len = get_larbs_info(log_str);
		tmp_str = log_str;
		if (len > 0) {
			while ((ptr = strsep(&tmp_str, "\n")) != NULL)
				pr_notice("%s\n", ptr);
		} else
			pr_notice("no larbs info to print\n");
		kfree(log_str);
	} else
		pr_notice("kmalloc fails!\n");
}

int get_dump_larbs(char *buf, const struct kernel_param *kp)
{
	int len;

	smi_debug_bus_hang_detect(false, "MMDVFS");
	len = get_larbs_info(buf);
	return len;
}

static struct kernel_param_ops dump_larb_param_ops = {
	.get = get_dump_larbs,
	.set = param_set_uint,
};
module_param_cb(dump_larbs, &dump_larb_param_ops, &dump_larbs, 0644);
MODULE_PARM_DESC(dump_larbs, "dump mmdvfs current larb setting");

int get_larb_mode(char *buf, const struct kernel_param *kp)
{
	int length = 0;

	length += snprintf(buf + length, PAGE_SIZE - length,
		"current mode: %d\n", larb_soft);
	length += snprintf(buf + length, PAGE_SIZE - length,
		"force mode: %d\n", force_larb_mode);
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops larb_mode_ops = {
	.get = get_larb_mode,
	.set = param_set_int,
};
module_param_cb(larb_mode, &larb_mode_ops, &force_larb_mode, 0644);
MODULE_PARM_DESC(larb_mode, "set or get current larb mode");

#define UT_MAX_REQUEST 10
static s32 qos_ut_case;
static struct plist_head ut_req_list;
static bool ut_req_init;
struct mm_qos_request ut_req[UT_MAX_REQUEST] = {};
static DECLARE_COMPLETION(comp);

static int test_event(struct notifier_block *nb,
		unsigned long value, void *v)
{
	pr_notice("ut test notifier: value=%lu\n", value);
	/*msleep(50);*/ /* Use it when disp's notifier callback not ready*/
	complete(&comp);
	return 0;
}
static struct notifier_block test_notifier = {
	.notifier_call = test_event,
};

static int make_cam_hrt_bw(void *data)
{
	struct plist_head cam_req_list;
	struct mm_qos_request cam_req = {};

	plist_head_init(&cam_req_list);
	mm_qos_add_request(&cam_req_list,
		&cam_req, SMI_PMQOS_ENC(cam_larb_ids[0], 0));
	mm_qos_set_request(&cam_req, 100, 100, 0);
	mm_qos_update_all_request(&cam_req_list);
	mm_qos_update_all_request_zero(&cam_req_list);
	mm_qos_remove_all_request(&cam_req_list);
	return 0;
}

int mmdvfs_qos_ut_set(const char *val, const struct kernel_param *kp)
{
	int result, value;
	u32 old_log_level = log_level;
	u32 req_id, master;
	struct task_struct *pKThread;
	u64 start_jiffies;

	result = sscanf(val, "%d %d %i %d", &qos_ut_case,
		&req_id, &master, &value);
	if (result != 4) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	if (req_id >= UT_MAX_REQUEST) {
		pr_notice("invalid req_id: %u\n", req_id);
		return -EINVAL;
	}

	pr_notice("ut with (case_id,req_id,master,value)=(%d,%u,%#x,%d)\n",
		qos_ut_case, req_id, master, value);
	log_level = 1 << log_bw | 1 << log_smi_freq;
	if (!ut_req_init) {
		plist_head_init(&ut_req_list);
		ut_req_init = true;
	}
	switch (qos_ut_case) {
	case 0:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, 0, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 1:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, value, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 2:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_bw_request(&ut_req[req_id], value, BW_COMP_NONE);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 3:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_hrt_request(&ut_req[req_id], value);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 4:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value, 0, BW_COMP_DEFAULT);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 5:
		mm_qos_add_request(&ut_req_list, &ut_req[req_id], master);
		mm_qos_set_request(&ut_req[req_id], value,
			value, BW_COMP_DEFAULT);
		mm_qos_update_all_request(&ut_req_list);
		break;
	case 6:
		/* Test blocking mechanism */
		reinit_completion(&comp);
		mm_hrt_add_bw_throttle_notifier(&test_notifier);
		/* Make camera block and trigger an event sent to notifier */
		mmdvfs_set_max_camera_hrt_bw(2000);
		pKThread = kthread_run(make_cam_hrt_bw,
			NULL, "make_cam_hrt_bw");
		if (IS_ERR(pKThread))
			pr_notice("create cam hrt bw thread failed\n");
		/* Notifier will call complete */
		wait_for_completion(&comp);
		reinit_completion(&comp);
		start_jiffies = jiffies;
		mmdvfs_set_max_camera_hrt_bw(0);
		wait_for_completion(&comp);
		pr_notice("wait time should > 2000 msecs:%u\n",
			jiffies_to_msecs(jiffies-start_jiffies));
		mm_hrt_remove_bw_throttle_notifier(&test_notifier);
		break;
	case 7:
		mmdvfs_set_max_camera_hrt_bw(5400);
		make_cam_hrt_bw(NULL);
		mmdvfs_set_max_camera_hrt_bw(0);
		break;
	case -1:
		mm_qos_remove_all_request(&ut_req_list);
		break;
	case -2:
		mm_qos_update_all_request_zero(&ut_req_list);
		break;
	default:
		pr_notice("invalid case_id: %d\n", qos_ut_case);
		break;
	}

	pr_notice("Call SMI Dump API Begin\n");
	/* smi_debug_bus_hang_detect(false, "MMDVFS"); */
	pr_notice("Call SMI Dump API END\n");
	log_level = old_log_level;
	return 0;
}

static struct kernel_param_ops qos_ut_case_ops = {
	.set = mmdvfs_qos_ut_set,
	.get = param_get_int,
};
module_param_cb(qos_ut_case, &qos_ut_case_ops, &qos_ut_case, 0644);
MODULE_PARM_DESC(qos_ut_case, "force mmqos UT test case");

int set_disp_bw_ceiling(const char *val, const struct kernel_param *kp)
{
	int result;
	s32 disp_bw, wait;
	s32 disp_avail_hrt_bw;


	result = sscanf(val, "%d %d", &disp_bw, &wait);
	if (result != 2) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	pr_notice("%s (disp_bw, wait): (%d,%d)\n",
		__func__, disp_bw, wait);

	disp_bw_ceiling = (disp_bw < 0)?0:disp_bw;
	wait_next_max_cam_bw_set = wait;

	disp_avail_hrt_bw = mm_hrt_get_available_hrt_bw(PORT_VIRTUAL_DISP);
	pr_notice("disp_bw_ceiling=%d total_hrt_bw=%d disp_avail_hrt_bw=%d\n",
		disp_bw_ceiling, total_hrt_bw, disp_avail_hrt_bw);

	if (!wait_next_max_cam_bw_set)
		blocking_notifier_call_chain(
			&hrt_bw_throttle_notifier,
			BW_THROTTLE_START, NULL);

	return 0;
}

static struct kernel_param_ops disp_bw_ceiling_ops = {
	.set = set_disp_bw_ceiling,
	.get = param_get_int,
};
module_param_cb(disp_bw_ceiling, &disp_bw_ceiling_ops,
	&disp_bw_ceiling, 0644);
MODULE_PARM_DESC(disp_bw_ceiling,
	"set display bw to test repaint and decouple");

int set_force_bwl(const char *val, const struct kernel_param *kp)
{
	int result;
	int comm, port, bwl;

	result = sscanf(val, "%d %d %d", &comm, &port, &bwl);
	if (result != 3) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}

	if (comm >= 0 && comm < MAX_COMM_NUM
		&& port >= 0 && port < SMI_COMM_MASTER_NUM)
		force_comm_bwl[comm][port] = bwl;

	return 0;
}

int get_force_bwl(char *buf, const struct kernel_param *kp)
{
	int i, j, length = 0;

	for (i = 0; i < MAX_COMM_NUM; i++)
		for (j = 0; j < SMI_COMM_MASTER_NUM; j++)
			length += snprintf(buf + length, PAGE_SIZE - length,
			"%d ", force_comm_bwl[i][j]);

	length += snprintf(buf + length, PAGE_SIZE - length, "\n");

	return length;
}

static struct kernel_param_ops force_bwl_ops = {
	.set = set_force_bwl,
	.get = get_force_bwl,
};
module_param_cb(force_bwl, &force_bwl_ops,
	NULL, 0644);
MODULE_PARM_DESC(force_bwl,
	"force bwl for each larb");

late_initcall(mmqos_late_init);
module_init(mmqos_init);
module_exit(mmqos_exit);

MODULE_DESCRIPTION("MTK MMQOS driver");
MODULE_AUTHOR("Damon Chu<damon.chu@mediatek.com>");
MODULE_LICENSE("GPL");
#else
#include <linux/string.h>
#include <linux/math64.h>
#include "mmdvfs_pmqos.h"
struct mm_qos_request *req;

void mm_qos_update_larb_bwl(u32 larb_update, bool bw_change)
{
}

s32 mm_qos_add_request(struct plist_head *owner_list,
	struct mm_qos_request *req, u32 smi_master_id)
{
	return 0;
}

s32 mm_qos_set_request(struct mm_qos_request *req, u32 bw_value,
	u32 hrt_value, u32 comp_type)
{
	return 0;
}

s32 mm_qos_set_bw_request(struct mm_qos_request *req,
	u32 bw_value, s32 comp_type)
{
	return 0;
}

s32 mm_qos_set_hrt_request(struct mm_qos_request *req,
	u32 hrt_value)
{
	return 0;
}

void mm_qos_update_all_request(struct plist_head *owner_list)
{
}

void mm_qos_remove_all_request(struct plist_head *owner_list)
{
}

void mm_qos_update_all_request_zero(struct plist_head *owner_list)
{
}

s32 mm_hrt_get_available_hrt_bw(u32 master_id)
{
	return 0;
}

s32 mm_hrt_add_bw_throttle_notifier(struct notifier_block *nb)
{
	return 0;
}

s32 mm_hrt_remove_bw_throttle_notifier(struct notifier_block *nb)
{
	return 0;
}

void mmdvfs_set_max_camera_hrt_bw(u32 bw)
{
}

void mmdvfs_print_larbs_info(void)
{
}

s32 get_virtual_port(enum virtual_source_id id)
{
	return 0;
}
#endif
