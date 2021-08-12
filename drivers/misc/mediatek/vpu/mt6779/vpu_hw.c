// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/ctype.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif
#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include <linux/scatterlist.h>
#endif
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>
#ifdef MTK_VPU_SMI_DEBUG_ON
#include <smi_debug.h>
#endif
#include <m4u.h>
#ifdef CONFIG_MTK_DEVINFO
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>
#endif

#ifdef MTK_PERF_OBSERVER
#include <mt-plat/mtk_perfobserver.h>
#endif

#define VPU_TRACE_ENABLED

/* #define BYPASS_M4U_DBG */

#include "vpu_hw.h"
#include "vpu_reg.h"
#include "vpu_cmn.h"
#include "vpu_algo.h"
#include "vpu_dbg.h"
#include "vpu_qos.h"
#include "vpu_dump.h"

#define ENABLE_PMQOS

#ifdef CONFIG_PM_SLEEP
struct wakeup_source *vpu_wake_lock[MTK_VPU_CORE];
#endif

#include "vpu_dvfs.h"
#include "apu_dvfs.h"
#include <linux/regulator/consumer.h>
#include "vpu_drv.h"
#include "mdla_dvfs.h"
#include "mtk_qos_bound.h"
#include <linux/pm_qos.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

static uint32_t g_efuse_data;
static uint32_t g_efuse_segment;
static inline uint32_t get_devinfo_with_index(int idx)
{
	uint32_t data = 0x0;

	if (idx == 3)
		data = g_efuse_data;

	if (idx == 7)
		data = g_efuse_segment;

	return data;
}

/* opp, mW */
struct VPU_OPP_INFO vpu_power_table[VPU_OPP_NUM] = {
	{VPU_OPP_0, 1165},
	{VPU_OPP_1, 1038},
	{VPU_OPP_2, 1008},
	{VPU_OPP_3, 988},
	{VPU_OPP_4, 932},
	{VPU_OPP_5, 658},
	{VPU_OPP_6, 564},
	{VPU_OPP_7, 521},
	{VPU_OPP_8, 456},
	{VPU_OPP_9, 315},
	{VPU_OPP_10, 275},
	{VPU_OPP_11, 210},
	{VPU_OPP_12, 138},


};

#include <linux/ktime.h>

#define CMD_WAIT_TIME_MS    (3 * 1000)
#define OPP_WAIT_TIME_MS    (300)
#define PWR_KEEP_TIME_MS    (2000)
#define OPP_KEEP_TIME_MS    (3000)
#define SDSP_KEEP_TIME_MS   (5000)
#define IOMMU_VA_START      (0x7DA00000)
#define IOMMU_VA_END        (0x82600000)
#define POWER_ON_MAGIC		(2)
#define OPPTYPE_VCORE		(0)
#define OPPTYPE_DSPFREQ		(1)
#define OPPTYPE_VVPU		(2)
#define OPPTYPE_VMDLA		(3)



/* 20180703, 00:00 : vpu log mechanism */
#define HOST_VERSION	(0x18070300)

/* ion & m4u */
#ifdef CONFIG_MTK_M4U
static struct m4u_client_t *m4u_client;
#endif
static struct ion_client *ion_client;
static struct vpu_device *vpu_dev;
static wait_queue_head_t cmd_wait;

struct vup_service_info {
	struct task_struct *srvc_task;
	struct sg_table sg_reset_vector;
	struct sg_table sg_main_program;
	struct sg_table sg_algo_binary_data;
	struct sg_table sg_iram_data;
	uint64_t vpu_base;
	uint64_t bin_base;
	uint64_t iram_data_mva;
	uint64_t algo_data_mva;
	vpu_id_t current_algo;
	vpu_id_t default_algo_num;
	int thread_var;
	struct mutex cmd_mutex;
	bool is_cmd_done;
	struct mutex state_mutex;
	enum VpuCoreState state;

	/* working buffer */
	struct vpu_shared_memory *work_buf;

	/* execution kernel library */
	struct vpu_shared_memory *exec_kernel_lib;
};
static struct vup_service_info vpu_service_cores[MTK_VPU_CORE];
struct vpu_shared_memory *core_shared_data; /* shared data for all cores */
bool exception_isr_check[MTK_VPU_CORE];

#ifndef MTK_VPU_FPGA_PORTING

#define VPU_EXCEPTION_MAGIC 0x52310000
static uint32_t vpu_dump_exception;

/* clock */
static struct clk *clk_top_dsp_sel;
static struct clk *clk_top_dsp1_sel;
static struct clk *clk_top_dsp2_sel;
//static struct clk *clk_top_dsp3_sel;
static struct clk *clk_top_ipu_if_sel;
static struct clk *clk_apu_core0_jtag_cg;
static struct clk *clk_apu_core0_axi_m_cg;
static struct clk *clk_apu_core0_apu_cg;
static struct clk *clk_apu_core1_jtag_cg;
static struct clk *clk_apu_core1_axi_m_cg;
static struct clk *clk_apu_core1_apu_cg;
static struct clk *clk_apu_conn_apu_cg;
static struct clk *clk_apu_conn_ahb_cg;
static struct clk *clk_apu_conn_axi_cg;
static struct clk *clk_apu_conn_isp_cg;
static struct clk *clk_apu_conn_cam_adl_cg;
static struct clk *clk_apu_conn_img_adl_cg;
static struct clk *clk_apu_conn_emi_26m_cg;
static struct clk *clk_apu_conn_vpu_udi_cg;
static struct clk *clk_apu_vcore_ahb_cg;
static struct clk *clk_apu_vcore_axi_cg;
static struct clk *clk_apu_vcore_adl_cg;
static struct clk *clk_apu_vcore_qos_cg;

static struct clk *clk_top_clk26m;
static struct clk *clk_top_univpll_d3_d8;
static struct clk *clk_top_univpll_d3_d4;
static struct clk *clk_top_mainpll_d2_d4;
static struct clk *clk_top_univpll_d3_d2;
static struct clk *clk_top_mainpll_d2_d2;
static struct clk *clk_top_univpll_d2_d2;
static struct clk *clk_top_mainpll_d3;
static struct clk *clk_top_univpll_d3;
static struct clk *clk_top_mmpll_d7;
static struct clk *clk_top_mmpll_d6;
static struct clk *clk_top_adsppll_d5;
static struct clk *clk_top_tvdpll_ck;
static struct clk *clk_top_tvdpll_mainpll_d2_ck;
static struct clk *clk_top_univpll_d2;
static struct clk *clk_top_adsppll_d4;
static struct clk *clk_top_mainpll_d2;
static struct clk *clk_top_mmpll_d4;



/* mtcmos */
static struct clk *mtcmos_dis;

//static struct clk *mtcmos_vpu_vcore_dormant;
static struct clk *mtcmos_vpu_vcore_shutdown;
//static struct clk *mtcmos_vpu_conn_dormant;
static struct clk *mtcmos_vpu_conn_shutdown;

//static struct clk *mtcmos_vpu_core0_dormant;
static struct clk *mtcmos_vpu_core0_shutdown;
//static struct clk *mtcmos_vpu_core1_dormant;
static struct clk *mtcmos_vpu_core1_shutdown;
//static struct clk *mtcmos_vpu_core2_dormant;
static struct clk *mtcmos_vpu_core2_shutdown;

/* smi */
static struct clk *clk_mmsys_gals_ipu2mm;
static struct clk *clk_mmsys_gals_ipu12mm;
static struct clk *clk_mmsys_gals_comm0;
static struct clk *clk_mmsys_gals_comm1;
static struct clk *clk_mmsys_smi_common;

/*direct link*/
//static struct clk *clk_mmsys_ipu_dl_txck;
//static struct clk *clk_mmsys_ipu_dl_rx_ck;

#endif

/* workqueue */
struct my_struct_t {
	int core;
	struct delayed_work my_work;
};
static struct workqueue_struct *wq;
static void vpu_power_counter_routine(struct work_struct *);
static struct my_struct_t power_counter_work[MTK_VPU_CORE];

/* static struct workqueue_struct *opp_wq; */
static void vpu_opp_keep_routine(struct work_struct *);
static void vpu_sdsp_routine(struct work_struct *work);
static DECLARE_DELAYED_WORK(opp_keep_work, vpu_opp_keep_routine);
static DECLARE_DELAYED_WORK(sdsp_work, vpu_sdsp_routine);


/* power */
static struct mutex power_mutex[MTK_VPU_CORE];
static bool is_power_on[MTK_VPU_CORE];
static bool is_power_debug_lock;
static struct mutex power_counter_mutex[MTK_VPU_CORE];
static int power_counter[MTK_VPU_CORE];
static struct mutex opp_mutex;
static bool force_change_vcore_opp[MTK_VPU_CORE];
static bool force_change_vvpu_opp[MTK_VPU_CORE];
static bool force_change_vmdla_opp[MTK_VPU_CORE];
static bool force_change_dsp_freq[MTK_VPU_CORE];
static bool change_freq_first[MTK_VPU_CORE];
static bool opp_keep_flag;
static bool sdsp_power_counter;
static wait_queue_head_t waitq_change_vcore;
static wait_queue_head_t waitq_do_core_executing;
static uint8_t max_vcore_opp;
static uint8_t max_vvpu_opp;
static uint8_t max_dsp_freq;
static struct mutex power_lock_mutex;
static struct mutex set_power_mutex;


/* dvfs */
static struct vpu_dvfs_opps opps;
#ifdef ENABLE_PMQOS
//static struct mtk_pm_qos_request vpu_qos_bw_request[MTK_VPU_CORE];
static struct mtk_pm_qos_request vpu_qos_vcore_request[MTK_VPU_CORE];
static struct mtk_pm_qos_request vpu_qos_vvpu_request[MTK_VPU_CORE];
static struct mtk_pm_qos_request vpu_qos_vmdla_request[MTK_VPU_CORE];
#endif

/* jtag */
static bool is_jtag_enabled;

/* direct link */
static bool is_locked;
static struct mutex lock_mutex;
static wait_queue_head_t lock_wait;

static struct vpu_lock_power lock_power[VPU_OPP_PRIORIYY_NUM][MTK_VPU_CORE];
static uint8_t max_opp[MTK_VPU_CORE];
static uint8_t min_opp[MTK_VPU_CORE];
static uint8_t max_boost[MTK_VPU_CORE];
static uint8_t min_boost[MTK_VPU_CORE];
static int vpu_init_done;
static uint8_t segment_max_opp;
static uint8_t segment_index;

static inline int atf_vcore_cg_ctl(int state)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_APU_VCORE_CG_CTL
			, state, 0, 0, 0, 0, 0, 0, &res);
	return 0;
}

/*move vcore cg ctl to atf*/
#define vcore_cg_ctl(poweron) \
	atf_vcore_cg_ctl(poweron)

/* isr handler */
static irqreturn_t vpu0_isr_handler(int irq, void *dev_id);
static irqreturn_t vpu1_isr_handler(int irq, void *dev_id);

/*regulator id*/
static struct regulator *vvpu_reg_id; /*Check Serivce reject set init vlue*/
static struct regulator *vmdla_reg_id;


typedef irqreturn_t (*ISR_CB)(int, void *);
struct ISR_TABLE {
	ISR_CB          isr_fp;
	unsigned int    int_number;
	char            device_name[16];
};

/* Must be the same name with that in device node. */
const struct ISR_TABLE VPU_ISR_CB_TBL[MTK_VPU_CORE] = {
	{vpu0_isr_handler,     0,  "ipu1"},
	{vpu1_isr_handler,     0,  "ipu2"}
};

static inline void lock_command(int core_s, int cmd)
{
	unsigned int core = (unsigned int)core_s;
	mutex_lock(&(vpu_service_cores[core].cmd_mutex));
	vpu_service_cores[core].is_cmd_done = false;
	vpu_write_field(core, FLD_XTENSA_INFO17, 0);
	LOG_INF("%s: vpu%d: cmd: %02xh, info00:%xh, info17:%xh\n",
		__func__, core, cmd,
		vpu_read_field(core, FLD_XTENSA_INFO00),
		vpu_read_field(core, FLD_XTENSA_INFO17));
}

static void vpu_err_msg(int core, const char *msg)
{
	LOG_ERR("%s: (%d)(%d)(%d)(%d.%d.%d.%d)(%d/%d)(%d/%d/%d)%d\n",
		msg,
		core,
		is_power_debug_lock,
		opps.vvpu.index,
		opps.dsp.index,
		opps.dspcore[0].index,
		opps.dspcore[1].index,
		opps.ipu_if.index,
		max_vvpu_opp,
		max_dsp_freq,
		force_change_vvpu_opp[core],
		force_change_dsp_freq[core],
		change_freq_first[core],
		opp_keep_flag);
}

#define vpu_err_hnd(hw_fail, core, req, key, fmt, args...) \
	do { \
		pr_info(fmt, ##args); \
		vpu_err_msg(core, __func__); \
		if (hw_fail) { \
			vpu_dmp_create_locked(core, req, fmt, ##args); \
			apu_get_power_info(); \
			vvpu_vmdla_vcore_checker(); \
			aee_kernel_exception("VPU", \
			"\nCRDISPATCH_KEY:" key "\n" fmt, ##args); \
		} \
	} while (0)

static void vpu_status(int core)
{
	/* TODO: touch memory */
}

static int wait_idle(int core, uint32_t latency, uint32_t retry)
{
	uint32_t pwait = 0;
	unsigned int count = 0;

	do {
		count++;
		pwait = vpu_read_field(core, FLD_PWAITMODE);
		if (pwait)
			return 0;
		udelay(latency);
	} while (count < retry);

	pr_info("%s: vpu%d: %d us: pwaitmode: %d, info00: 0x%x, info25: 0x%x\n",
		__func__, core, (latency * retry), pwait,
		vpu_read_field(core, FLD_XTENSA_INFO00),
		vpu_read_field(core, FLD_XTENSA_INFO25));

	return -ETIMEDOUT;
}

static inline int wait_command(int core_s)
{
	int ret = 0;
	int count = 0;
	bool retry = true;
	unsigned int core = (unsigned int)core_s;

#define CMD_WAIT_STEP_MS 1000
#define CMD_WAIT_COUNT (CMD_WAIT_TIME_MS / CMD_WAIT_STEP_MS)

start:
	ret = wait_event_interruptible_timeout(cmd_wait,
				vpu_service_cores[core].is_cmd_done,
				msecs_to_jiffies(CMD_WAIT_STEP_MS));

	/* ret == -ERESTARTSYS, if signal interrupt */
	if (ret == -ERESTARTSYS) {
		pr_info("%s: vpu%d: interrupt by signal: ret=%d\n",
			__func__, core, ret);

		if (retry) {
			pr_info("%s: vpu%d: try wait again\n",
				__func__, core);
			retry = false;
			goto start;
		}
		goto out;
	}

	++count;
	if (ret) {  /* condition true: cmd done */
		ret = 0;
	} else {    /* condition false: timeout or retry*/
		if (count >= CMD_WAIT_COUNT) {
			pr_info("%s: vpu%d: timeout: %d ms\n",
				__func__, core, CMD_WAIT_TIME_MS);
			ret = -ETIMEDOUT;
		} else {
			vpu_status(core);
			goto start;
		}
	}

	if (ret < 0)
		ret = wait_idle(core, 2000 /* ms */, 5 /* times */);
out:
	return ret;
}

static inline void unlock_command(int core_s)
{
	unsigned int core = (unsigned int)core_s;
	mutex_unlock(&(vpu_service_cores[core].cmd_mutex));
}

static inline int Map_DSP_Freq_Table(int freq_opp)
{
	int freq_value = 0;

	switch (freq_opp) {
	case 0:
	default:
		freq_value = 700;
		break;
	case 1:
		freq_value = 624;
		break;
	case 2:
		freq_value = 606;
		break;
	case 3:
		freq_value = 594;
		break;
	case 4:
		freq_value = 560;
		break;
	case 5:
		freq_value = 525;
		break;
	case 6:
		freq_value = 450;
		break;
	case 7:
		freq_value = 416;
		break;
	case 8:
		freq_value = 364;
		break;
	case 9:
		freq_value = 312;
		break;
	case 10:
		freq_value = 273;
		break;
	case 11:
		freq_value = 208;
		break;
	case 12:
		freq_value = 137;
		break;
	case 13:
		freq_value = 104;
		break;
	case 14:
		freq_value = 52;
		break;
	case 15:
		freq_value = 26;
		break;
	}

	return freq_value;
}

/******************************************************************************
 * Add MET ftrace event for power profilling.
 *****************************************************************************/
#if defined(VPU_MET_READY)
void MET_Events_DVFS_Trace(void)
{
	int vcore_opp = 0;
	int dsp_freq = 0, ipu_if_freq = 0, dsp1_freq = 0, dsp2_freq = 0;

	mutex_lock(&opp_mutex);
		vcore_opp = opps.vcore.index;
		dsp_freq = Map_DSP_Freq_Table(opps.dsp.index);
		ipu_if_freq = Map_DSP_Freq_Table(opps.ipu_if.index);
		dsp1_freq = Map_DSP_Freq_Table(opps.dspcore[0].index);
		dsp2_freq = Map_DSP_Freq_Table(opps.dspcore[1].index);
	mutex_unlock(&opp_mutex);
	vpu_met_event_dvfs(vcore_opp, dsp_freq, ipu_if_freq,
		dsp1_freq, dsp2_freq);
}

void MET_Events_Trace(bool enter, int core_s, int algo_id)
{
	unsigned int core = (unsigned int)core_s;
	if (enter) {
		int dsp_freq = 0;

		mutex_lock(&opp_mutex);
		dsp_freq = Map_DSP_Freq_Table(opps.dspcore[core].index);
		mutex_unlock(&opp_mutex);
		vpu_met_event_enter(core, algo_id, dsp_freq);
	} else {
		vpu_met_event_leave(core, algo_id);
	}
}
#endif

static inline bool vpu_other_core_idle_check(int core)
{
	int i = 0;
	bool idle = true;

	LOG_DBG("vpu %s+\n", __func__);

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (i == core) {
			continue;
		} else {
			LOG_DBG("vpu test %d/%d/%d\n", core,
					i, vpu_service_cores[i].state);

			mutex_lock(&(vpu_service_cores[i].state_mutex));
			switch (vpu_service_cores[i].state) {
			case VCT_SHUTDOWN:
			case VCT_BOOTUP:
			case VCT_IDLE:
				break;
			case VCT_EXECUTING:
			case VCT_NONE:
			case VCT_VCORE_CHG:
				idle = false;
				mutex_unlock(
					&(vpu_service_cores[i].state_mutex));
				goto out;
				/*break;*/
			}
			mutex_unlock(&(vpu_service_cores[i].state_mutex));
		}
	}

	LOG_DBG("vpu core_idle_check %d, %d/%d\n", idle,
			vpu_service_cores[0].state,
			vpu_service_cores[1].state);
out:
	return idle;
}

static inline int wait_to_do_change_vcore_opp(int core)
{
	int ret = 0;
	int retry = 0;

	/* now this function just return directly. NO WAIT */
	if (g_func_mask & VFM_NEED_WAIT_VCORE) {
		if (g_vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[vpu_%d_0x%x] wait for vcore change now\n",
					core, g_func_mask);
		}
	} else {
		return ret;
	}

	do {
		ret = wait_event_interruptible_timeout(waitq_change_vcore,
					vpu_other_core_idle_check(core),
					msecs_to_jiffies(OPP_WAIT_TIME_MS));

		/* ret == -ERESTARTSYS, if signal interrupt */
		if (ret < 0) {
			retry += 1;
			ret = -EINTR;
			LOG_ERR("[vpu_%d/%d] %s, %s, ret=%d\n",
				core, retry,
				"interrupt by signal",
				"while wait to change vcore",
				ret);
		} else {
			/* normal case */
			if (ret > 0) {
				ret = 0;
			} else {
				ret = -ETIMEDOUT;
				LOG_ERR("[vpu_%d] %s timeout, ret=%d\n", core,
					__func__, ret);
			}
			break;
		}
	} while (retry < 3);

	return ret;
}

static inline bool vpu_opp_change_idle_check(int core)
{
	int i = 0;
	bool idle = true;

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (i == core) {
			continue;
		} else {
			LOG_DBG("vpu test %d/%d/%d\n", core, i,
					vpu_service_cores[i].state);

			mutex_lock(&(vpu_service_cores[i].state_mutex));
			switch (vpu_service_cores[i].state) {
			case VCT_SHUTDOWN:
			case VCT_BOOTUP:
			case VCT_IDLE:
			case VCT_EXECUTING:
			case VCT_NONE:
				break;
			case VCT_VCORE_CHG:
				idle = false;
				mutex_unlock(
					&(vpu_service_cores[i].state_mutex));
				goto out;
				/*break;*/
			}
			mutex_unlock(&(vpu_service_cores[i].state_mutex));
		}
	}

out:
	return idle;
}

static inline int wait_to_do_vpu_running(int core)
{
	int ret = 0;
	int retry = 0;

	/* now this function just return directly. NO WAIT */
	if (g_func_mask & VFM_NEED_WAIT_VCORE) {
		if (g_vpu_log_level > Log_STATE_MACHINE) {
			LOG_INF("[vpu_%d_0x%x] wait for vpu running now\n",
					core, g_func_mask);
		}
	} else {
		return ret;
	}

	do {
		ret = wait_event_interruptible_timeout(waitq_do_core_executing,
					vpu_opp_change_idle_check(core),
					msecs_to_jiffies(OPP_WAIT_TIME_MS));

		/* ret == -ERESTARTSYS, if signal interrupt */
		if (ret < 0) {
			retry += 1;
			ret = -EINTR;
			LOG_ERR("[vpu_%d/%d] %s, %s, ret=%d\n",
				core, retry,
				"interrupt by signal",
				"while wait to do vpu running",
				ret);
		} else {
			/* normal case */
			if (ret > 0) {
				ret = 0;
			} else {
				ret = -ETIMEDOUT;
				LOG_ERR("[vpu_%d] %s timeout, ret=%d\n",
					core, __func__, ret);
			}
			break;
		}
	} while (retry < 3);

	return ret;
}

#ifndef MTK_VPU_FPGA_PORTING
static int vpu_set_clock_source(struct clk *clk, uint8_t step)
{
	struct clk *clk_src;

	LOG_DBG("vpu scc(%d)", step);
	/* set dsp frequency - 0:700 MHz, 1:624 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/

	switch (step) {
	case 0:
		clk_src = clk_top_adsppll_d4;
		break;
	case 1:
		clk_src = clk_top_univpll_d2;
		break;
	case 2:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;
	case 3:
		clk_src = clk_top_tvdpll_ck;
		break;
	case 4:
		clk_src = clk_top_adsppll_d5;
		break;
	case 5:
		clk_src = clk_top_mmpll_d6;
		break;
	case 6:
		clk_src = clk_top_mmpll_d7;
		break;
	case 7:
		clk_src = clk_top_univpll_d3;
		break;
	case 8:
		clk_src = clk_top_mainpll_d3;
		break;
	case 9:
		clk_src = clk_top_univpll_d2_d2;
		break;
	case 10:
		clk_src = clk_top_mainpll_d2_d2;
		break;
	case 11:
		clk_src = clk_top_univpll_d3_d2;
		break;
	case 12:
		clk_src = clk_top_mainpll_d2_d4;
		break;
	case 13:
		clk_src = clk_top_univpll_d3_d4;
		break;
	case 14:
		clk_src = clk_top_univpll_d3_d8;
		break;
	case 15:
		clk_src = clk_top_clk26m;
		break;
	default:
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}

	return clk_set_parent(clk, clk_src);
}
/*set CONN hf_fdsp_ck, VCORE hf_fipu_if_ck*/
static int vpu_if_set_clock_source(struct clk *clk, uint8_t step)
{
	struct clk *clk_src;

	LOG_DBG("vpu scc(%d)", step);
	/* set dsp frequency - 0:624 MHz, 1:624 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/
	switch (step) {
	case 0:
		clk_src = clk_top_univpll_d2;/*624MHz*/
		break;
	case 1:
		clk_src = clk_top_univpll_d2;/*624MHz*/
		break;
	case 2:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;
	case 3:
		clk_src = clk_top_tvdpll_ck;
		break;
	case 4:
		clk_src = clk_top_adsppll_d5;
		break;
	case 5:
		clk_src = clk_top_mmpll_d6;
		break;
	case 6:
		clk_src = clk_top_mmpll_d7;
		break;
	case 7:
		clk_src = clk_top_univpll_d3;
		break;
	case 8:
		clk_src = clk_top_mainpll_d3;
		break;
	case 9:
		clk_src = clk_top_univpll_d2_d2;
		break;
	case 10:
		clk_src = clk_top_mainpll_d2_d2;
		break;
	case 11:
		clk_src = clk_top_univpll_d3_d2;
		break;
	case 12:
		clk_src = clk_top_mainpll_d2_d4;
		break;
	case 13:
		clk_src = clk_top_univpll_d3_d4;
		break;
	case 14:
		clk_src = clk_top_univpll_d3_d8;
		break;
	case 15:
		clk_src = clk_top_clk26m;
		break;
	default:
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}

	return clk_set_parent(clk, clk_src);
}

#endif
int get_vpu_bw(int core)
{
	struct qos_bound *bound = get_qos_bound();
	int bw = 0;

	if (core == 0)
		bw = bound->stats[bound->idx].smibw_mon[QOS_SMIBM_VPU0];
	else
		bw = bound->stats[bound->idx].smibw_mon[QOS_SMIBM_VPU1];

	LOG_INF("[vpu] bw(%d)", bw);
	return bw;
}
int get_vpu_latency(int core)
{
	struct qos_bound *bound = get_qos_bound();
	int lat = 0;

	if (core == 0)
		lat = bound->stats[bound->idx].lat_mon[QOS_LAT_VPU0];
	else
		lat = bound->stats[bound->idx].lat_mon[QOS_LAT_VPU1];
	LOG_INF("[vpu] lat(%d)", lat);
	return lat;
}

int get_vpu_opp(void)
{
	//LOG_DBG("[mdla_%d] vvpu(%d->%d)\n", core, get_vvpu_value, opp_value);
	return opps.dsp.index;
}
EXPORT_SYMBOL(get_vpu_opp);

int get_vpu_dspcore_opp(int core_s)
{
	unsigned int core = (unsigned int)core_s;
	LOG_DBG("[vpu_%d] get opp:%d\n", core, opps.dspcore[core].index);
	return opps.dspcore[core].index;
}
EXPORT_SYMBOL(get_vpu_dspcore_opp);

int get_vpu_platform_floor_opp(void)
{
	return (VPU_MAX_NUM_OPPS - 1);
}
EXPORT_SYMBOL(get_vpu_platform_floor_opp);

int get_vpu_ceiling_opp(int core_s)
{
	unsigned int core = (unsigned int)core_s;
	return max_opp[core];
}
EXPORT_SYMBOL(get_vpu_ceiling_opp);

int get_vpu_opp_to_freq(uint8_t step)
{
	int freq = 0;
	/* set dsp frequency - 0:700 MHz, 1:624 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/



	switch (step) {
	case 0:
		freq = 700;
		break;
	case 1:
		freq = 624;
		break;
	case 2:
		freq = 606;
		break;
	case 3:
		freq = 594;
		break;
	case 4:
		freq = 560;
		break;
	case 5:
		freq = 525;
		break;
	case 6:
		freq = 450;
		break;
	case 7:
		freq = 416;
		break;
	case 8:
		freq = 364;
		break;
	case 9:
		freq = 312;
		break;
	case 10:
		freq = 273;
		break;
	case 11:
		freq = 208;
		break;
	case 12:
		freq = 137;
		break;
	case 13:
		freq = 104;
		break;
	case 14:
		freq = 52;
		break;
	case 15:
		freq = 26;
		break;
	default:
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}
	return freq;
}
EXPORT_SYMBOL(get_vpu_opp_to_freq);

int get_vpu_init_done(void)
{
	return vpu_init_done;
}
EXPORT_SYMBOL(get_vpu_init_done);

static void get_segment_from_efuse(void)
{
	int segment = 0;

	segment = get_devinfo_with_index(7) & 0xFF;
	switch (segment) {
	case 0x7://segment p90M 5mode
		segment_max_opp = 0;
		segment_index = SEGMENT_90M;
		break;
	case 0xE0://segment p90M 6mode 525M
		segment_max_opp = 0;
		segment_index = SEGMENT_90M;
		break;
	case 0x20://p95
	case 0x4:
	case 0x60:
	case 0x6:
	case 0x10:
	case 0x8:
	case 0x90:
	case 0x9:
		segment_max_opp = 0;
		segment_index = SEGMENT_95;
		break;
	default: //segment p90
		segment_max_opp = 0;
		segment_index = SEGMENT_90;
		break;
	}
	LOG_INF("vpu segment_max_opp: %d\n", segment_max_opp);
}

/* expected range, vvpu_index: 0~15 */
/* expected range, freq_index: 0~15 */
//static void vpu_opp_check(int core, uint8_t vcore_index, uint8_t freq_index)
static void vpu_opp_check(int core_s, uint8_t vvpu_index, uint8_t freq_index)
{
	int i = 0;
	bool freq_check = false;
	int log_freq = 0, log_max_freq = 0;
	//int get_vcore_opp = 0;
	int get_vvpu_opp = 0;
	unsigned int core = (unsigned int)core_s;

	if (is_power_debug_lock) {
		force_change_vcore_opp[core] = false;
		force_change_vvpu_opp[core] = false;
		force_change_dsp_freq[core] = false;
		goto out;
	}

	log_freq = Map_DSP_Freq_Table(freq_index);

	LOG_DBG("opp_check + (%d/%d/%d), ori vvpu(%d)\n", core,
			vvpu_index, freq_index, opps.vvpu.index);

	mutex_lock(&opp_mutex);
	change_freq_first[core] = false;
	log_max_freq = Map_DSP_Freq_Table(max_dsp_freq);
	/*segment limitation*/
	if (vvpu_index < opps.vvpu.opp_map[segment_max_opp])
		vvpu_index = opps.vvpu.opp_map[segment_max_opp];
	if (freq_index < segment_max_opp)
		freq_index = segment_max_opp;
	/* vvpu opp*/
	get_vvpu_opp = vpu_get_hw_vvpu_opp(core);
	if (vvpu_index < opps.vvpu.opp_map[max_opp[core]])
		vvpu_index = opps.vvpu.opp_map[max_opp[core]];
	if (vvpu_index > opps.vvpu.opp_map[min_opp[core]])
		vvpu_index = opps.vvpu.opp_map[min_opp[core]];
	if (freq_index < max_opp[core])
		freq_index = max_opp[core];
	if (freq_index > min_opp[core])
		freq_index = min_opp[core];
	LOG_DVFS("opp_check + max_opp%d, min_opp%d,(%d/%d/%d), ori vvpu(%d)",
		max_opp[core], min_opp[core], core,
		vvpu_index, freq_index, opps.vvpu.index);

if (vvpu_index == 0xFF) {

	LOG_DBG("no need, vvpu opp(%d), hw vore opp(%d)\n",
			vvpu_index, get_vvpu_opp);

	force_change_vvpu_opp[core] = false;
	opps.vvpu.index = vvpu_index;
} else {
	/* opp down, need change freq first*/
	if (vvpu_index > get_vvpu_opp)
		change_freq_first[core] = true;

	if (vvpu_index < max_vvpu_opp) {
		LOG_INF("vpu bound vvpu opp(%d) to %d",
				vvpu_index, max_vvpu_opp);

		vvpu_index = max_vvpu_opp;
	}

	if (vvpu_index >= opps.count) {
		LOG_ERR("wrong vvpu opp(%d), max(%d)",
				vvpu_index, opps.count - 1);

	} else if ((vvpu_index < opps.vvpu.index) ||
			((vvpu_index > opps.vvpu.index) &&
				(!opp_keep_flag)) ||
				(mdla_get_opp() < opps.dsp.index) ||
				(vvpu_index < get_vvpu_opp) ||
				((opps.dspcore[core].index < 9) &&
				(get_vvpu_opp >= 2)) ||
				((opps.dspcore[core].index < 5) &&
				(get_vvpu_opp >= 1))) {
		opps.vvpu.index = vvpu_index;
		force_change_vvpu_opp[core] = true;
		freq_check = true;
	}
}

	/* dsp freq opp */
	if (freq_index == 0xFF) {
		LOG_DBG("no request, freq opp(%d)", freq_index);
		force_change_dsp_freq[core] = false;
	} else {
		if (freq_index < max_dsp_freq) {
			LOG_INF("vpu bound dsp freq(%dMHz) to %dMHz",
					log_freq, log_max_freq);
			freq_index = max_dsp_freq;
		}

		if ((opps.dspcore[core].index != freq_index) || (freq_check)) {
			/* freq_check for all vcore adjust related operation
			 * in acceptable region
			 */

			/* vcore not change and dsp change */
			//if ((force_change_vcore_opp[core] == false) &&
			if ((force_change_vvpu_opp[core] == false) &&
				(freq_index > opps.dspcore[core].index) &&
				(opp_keep_flag)) {
				if (g_vpu_log_level > Log_ALGO_OPP_INFO) {
					LOG_INF("%s(%d) %s (%d/%d_%d/%d)\n",
						__func__,
						core,
						"dsp keep high",
						force_change_vvpu_opp[core],
						freq_index,
						opps.dspcore[core].index,
						opp_keep_flag);
				}
			} else {
				opps.dspcore[core].index = freq_index;
					/*To FIX*/
				if (opps.vvpu.index == 1 &&
						opps.dspcore[core].index < 5) {
					/* adjust 0~3 to 4~7 for real table
					 * if needed
					 */
					opps.dspcore[core].index = 5;
				}
				if (opps.vvpu.index == 2 &&
						opps.dspcore[core].index < 9) {
					/* adjust 0~3 to 4~7 for real table
					 * if needed
					 */
					opps.dspcore[core].index = 9;
				}

				opps.dsp.index = 15;
				opps.ipu_if.index = 15;
				for (i = 0 ; i < MTK_VPU_CORE ; i++) {
					LOG_DBG("%s %s[%d].%s(%d->%d)\n",
						__func__,
						"opps.dspcore",
						core,
						"index",
						opps.dspcore[core].index,
						opps.dsp.index);

					/* interface should be the max freq of
					 * vpu cores
					 */
					if ((opps.dspcore[i].index <
						opps.dsp.index) &&
						(opps.dspcore[i].index >=
						max_dsp_freq)) {

						opps.dsp.index =
							opps.dspcore[i].index;

						opps.ipu_if.index = 9;
					}
#ifdef MTK_MDLA_SUPPORT
/* interface should be the max freq of vpu cores and mdla*/
			if (mdla_get_opp() < opps.dsp.index) {
				LOG_DBG("check mdla dsp.index\n");
				opps.dsp.index = mdla_get_opp();
				opps.ipu_if.index =	9;
			}
#endif
/*check opps.dsp.index and opps.vvpu.index again*/
			if (opps.dsp.index < 9) {
				if (opps.dsp.index < 5)
					opps.vvpu.index = 0;
				else
					opps.vvpu.index = 1;
				}
				}
				force_change_dsp_freq[core] = true;

				opp_keep_flag = true;
				mod_delayed_work(wq, &opp_keep_work,
					msecs_to_jiffies(OPP_KEEP_TIME_MS));
			}
		} else {
			/* vcore not change & dsp not change */
			if (g_vpu_log_level > Log_ALGO_OPP_INFO)
				LOG_INF("opp_check(%d) vcore/dsp no change\n",
						core);

			opp_keep_flag = true;

			mod_delayed_work(wq, &opp_keep_work,
					msecs_to_jiffies(OPP_KEEP_TIME_MS));
		}
	}
	mutex_unlock(&opp_mutex);
out:
	LOG_INF("%s(%d)(%d/%d_%d)(%d/%d)(%d.%d.%d.%d)(%d/%d)(%d/%d/%d/%d)%d\n",
		"opp_check",
		core,
		is_power_debug_lock,
		vvpu_index,
		freq_index,
		opps.vvpu.index,
		get_vvpu_opp,
		opps.dsp.index,
		opps.dspcore[0].index,
		opps.dspcore[1].index,
		opps.ipu_if.index,
		max_vvpu_opp,
		max_dsp_freq,
		freq_check,
		force_change_vvpu_opp[core],
		force_change_dsp_freq[core],
		change_freq_first[core],
		opp_keep_flag);
}

static bool vpu_change_opp(int core_s, int type)
{
#ifdef MTK_VPU_FPGA_PORTING
	LOG_INF("[vpu_%d] %d Skip at FPGA", core, type);

	return true;
#else
	int ret = false;
	unsigned int core = (unsigned int)core_s;

	switch (type) {
	/* vcore opp */
	case OPPTYPE_VCORE:
		LOG_DBG("[vpu_%d] wait for changing vcore opp", core);

		ret = wait_to_do_change_vcore_opp(core);
		if (ret) {
			LOG_ERR("[vpu_%d] timeout to %s, ret=%d\n",
				core,
				"wait_to_do_change_vcore_opp",
				ret);
			goto out;
		}
		LOG_DBG("[vpu_%d] to do vcore opp change", core);
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_VCORE_CHG;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		mutex_lock(&opp_mutex);
		vpu_trace_begin("vcore:request");
		#ifdef ENABLE_PMQOS
		switch (opps.vcore.index) {
		case 0:
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_0);
			break;
		case 1:
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_1);
			break;
		case 2:
		default:
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		}
		#else
		ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
							opps.vcore.index);
		#endif
		vpu_trace_end();
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_BOOTUP;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		if (ret) {
			LOG_ERR("[vpu_%d]fail to request vcore, step=%d\n",
					core, opps.vcore.index);
			goto out;
		}

		LOG_DBG("[vpu_%d] cgopp vvpu=%d\n",
				core,
				regulator_get_voltage(vvpu_reg_id));

		force_change_vcore_opp[core] = false;
		mutex_unlock(&opp_mutex);
		wake_up_interruptible(&waitq_do_core_executing);
		break;
	/* dsp freq opp */
	case OPPTYPE_DSPFREQ:
		mutex_lock(&opp_mutex);
		LOG_INF("[vpu_%d] %s setclksrc(%d/%d/%d/%d)\n",
				__func__,
				core,
				opps.dsp.index,
				opps.dspcore[0].index,
				opps.dspcore[1].index,
				opps.ipu_if.index);

		ret = vpu_if_set_clock_source(clk_top_dsp_sel, opps.dsp.index);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to set dsp freq, %s=%d, %s=%d\n",
				core,
				"step", opps.dsp.index,
				"ret", ret);
			goto out;
		}

		if (core == 0) {
			ret = vpu_set_clock_source(clk_top_dsp1_sel,
						opps.dspcore[core].index);
			if (ret) {
				LOG_ERR("[vpu_%d]%s%d freq, %s=%d, %s=%d\n",
					core,
					"fail to set dsp_",
					core,
					"step", opps.dspcore[core].index,
					"ret", ret);
				goto out;
			}
		} else if (core == 1) {
			ret = vpu_set_clock_source(clk_top_dsp2_sel,
						opps.dspcore[core].index);
			if (ret) {
				LOG_ERR("[vpu_%d]%s%d freq, %s=%d, %s=%d\n",
					core,
					"fail to set dsp_",
					core,
					"step", opps.dspcore[core].index,
					"ret", ret);
				goto out;
			}
		}

		ret = vpu_if_set_clock_source(clk_top_ipu_if_sel,
							opps.ipu_if.index);
		if (ret) {
			LOG_ERR("[vpu_%d]%s, %s=%d, %s=%d\n",
					core,
					"fail to set ipu_if freq",
					"step", opps.ipu_if.index,
					"ret", ret);
			goto out;
		}

		force_change_dsp_freq[core] = false;
		mutex_unlock(&opp_mutex);

#ifdef MTK_PERF_OBSERVER
		{
			struct pob_xpufreq_info pxi;

			pxi.id = core;
			pxi.opp = opps.dspcore[core].index;

			pob_xpufreq_update(POB_XPUFREQ_VPU, &pxi);
		}
#endif

		break;
	/* vvpu opp */
	case OPPTYPE_VVPU:
		LOG_DBG("[vpu_%d] wait for changing vvpu opp", core);
		LOG_DBG("[vpu_%d] to do vvpu opp change", core);
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_VCORE_CHG;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		mutex_lock(&opp_mutex);
		vpu_trace_begin("vcore:request");
		#ifdef ENABLE_PMQOS
		switch (opps.vvpu.index) {
		case 0:
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_0);
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_0);
			break;
		case 1:
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		case 2:
		default:
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_2);
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_2);
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		}
		#else
		ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
							opps.vcore.index);
		#endif
		vpu_trace_end();
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_BOOTUP;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		if (ret) {
			LOG_ERR("[vpu_%d]fail to request vcore, step=%d\n",
					core, opps.vcore.index);
			goto out;
		}

		LOG_DBG("[vpu_%d] cgopp vvpu=%d, vmdla=%d\n",
				core,
				regulator_get_voltage(vvpu_reg_id),
				regulator_get_voltage(vmdla_reg_id));

		force_change_vcore_opp[core] = false;
		force_change_vvpu_opp[core] = false;
		force_change_vmdla_opp[core] = false;
		mutex_unlock(&opp_mutex);
		wake_up_interruptible(&waitq_do_core_executing);
		break;
	default:
		LOG_DVFS("unexpected type(%d)", type);
		break;
	}

out:
#if defined(VPU_MET_READY)
	MET_Events_DVFS_Trace();
#endif
	return true;
#endif
}

int32_t vpu_thermal_en_throttle_cb(uint8_t vcore_opp, uint8_t vpu_opp)
{
	int i = 0;
	int ret = 0;
	int vvpu_opp_index = 0;
	int vpu_freq_index = 0;

	if (vpu_init_done != 1)
		return ret;

	if (vpu_opp < VPU_MAX_NUM_OPPS) {
		vvpu_opp_index = opps.vvpu.opp_map[vpu_opp];
		vpu_freq_index = opps.dsp.opp_map[vpu_opp];
	} else {
		LOG_ERR("vpu_thermal_en wrong opp(%d)\n", vpu_opp);
		return -1;
	}
	LOG_INF("%s, opp(%d)->(%d/%d)\n", __func__,
		vpu_opp, vvpu_opp_index, vpu_freq_index);

	mutex_lock(&opp_mutex);
	max_dsp_freq = vpu_freq_index;
	max_vvpu_opp = vvpu_opp_index;
	mutex_unlock(&opp_mutex);
	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		mutex_lock(&opp_mutex);

		/* force change for all core under thermal request */
		opp_keep_flag = false;

		mutex_unlock(&opp_mutex);
		vpu_opp_check(i, vvpu_opp_index, vpu_freq_index);
	}

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (force_change_dsp_freq[i]) {
			/* force change freq while running */
			switch (vpu_freq_index) {
			case 0:
			default:
				LOG_INF("thermal force bound freq @700MHz\n");
				break;
			case 1:
				LOG_INF("thermal force bound freq @ 624MHz\n");
				break;
			case 2:
				LOG_INF("thermal force bound freq @606MHz\n");
				break;
			case 3:
				LOG_INF("thermal force bound freq @594MHz\n");
				break;
			case 4:
				LOG_INF("thermal force bound freq @560MHz\n");
				break;
			case 5:
				LOG_INF("thermal force bound freq @525MHz\n");
				break;
			case 6:
				LOG_INF("thermal force bound freq @450MHz\n");
				break;
			case 7:
				LOG_INF("thermal force bound freq @416MHz\n");
				break;
			case 8:
				LOG_INF("thermal force bound freq @364MHz\n");
				break;
			case 9:
				LOG_INF("thermal force bound freq @312MHz\n");
				break;
			case 10:
				LOG_INF("thermal force bound freq @273MHz\n");
				break;
			case 11:
				LOG_INF("thermal force bound freq @208MHz\n");
				break;
			case 12:
				LOG_INF("thermal force bound freq @137MHz\n");
				break;
			case 13:
				LOG_INF("thermal force bound freq @104MHz\n");
				break;
			case 14:
				LOG_INF("thermal force bound freq @52MHz\n");
				break;
			case 15:
				LOG_INF("thermal force bound freq @26MHz\n");
				break;
			}
			/*vpu_change_opp(i, OPPTYPE_DSPFREQ);*/
		}
		if (force_change_vvpu_opp[i]) {
			/* vcore change should wait */
			LOG_INF("thermal force bound vcore opp to %d\n",
					vvpu_opp_index);
			/* vcore only need to change one time from
			 * thermal request
			 */
			/*if (i == 0)*/
			/*	vpu_change_opp(i, OPPTYPE_VCORE);*/
		}
	}

	return ret;
}

int32_t vpu_thermal_dis_throttle_cb(void)
{
	int ret = 0;

	if (vpu_init_done != 1)
		return ret;
	LOG_INF("%s +\n", __func__);
	mutex_lock(&opp_mutex);
	max_vcore_opp = 0;
	max_dsp_freq = 0;
	max_vvpu_opp = 0;
	mutex_unlock(&opp_mutex);
	LOG_INF("%s -\n", __func__);

	return ret;
}

static int vpu_prepare_regulator_and_clock(struct device *pdev)
{
	int ret = 0;
/*enable Vvpu Vmdla*/
/*--Get regulator handle--*/
	vvpu_reg_id = regulator_get(pdev, "vpu");
	if (!vvpu_reg_id) {
		ret = -ENOENT;
	    LOG_ERR("regulator_get vvpu_reg_id failed\n");
	}
	vmdla_reg_id = regulator_get(pdev, "VMDLA");
	if (!vmdla_reg_id) {
		ret = -ENOENT;
		LOG_ERR("regulator_get vmdla_reg_id failed\n");
	}

#ifdef MTK_VPU_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);
#else
#define PREPARE_VPU_MTCMOS(clk) \
	{ \
		clk = devm_clk_get(pdev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find mtcmos: %s\n", #clk); \
		} \
	}
	PREPARE_VPU_MTCMOS(mtcmos_dis);
	PREPARE_VPU_MTCMOS(mtcmos_vpu_vcore_shutdown);
	PREPARE_VPU_MTCMOS(mtcmos_vpu_conn_shutdown);
	PREPARE_VPU_MTCMOS(mtcmos_vpu_core0_shutdown);
	PREPARE_VPU_MTCMOS(mtcmos_vpu_core1_shutdown);
	PREPARE_VPU_MTCMOS(mtcmos_vpu_core2_shutdown);

#undef PREPARE_VPU_MTCMOS

#define PREPARE_VPU_CLK(clk) \
	{ \
		clk = devm_clk_get(pdev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find clock: %s\n", #clk); \
		} else if (clk_prepare(clk)) { \
			ret = -EBADE; \
			LOG_ERR("fail to prepare clock: %s\n", #clk); \
		} \
	}

	PREPARE_VPU_CLK(clk_mmsys_gals_ipu2mm);
	PREPARE_VPU_CLK(clk_mmsys_gals_ipu12mm);
	PREPARE_VPU_CLK(clk_mmsys_gals_comm0);
	PREPARE_VPU_CLK(clk_mmsys_gals_comm1);
	PREPARE_VPU_CLK(clk_mmsys_smi_common);
	PREPARE_VPU_CLK(clk_apu_vcore_ahb_cg);
	PREPARE_VPU_CLK(clk_apu_vcore_axi_cg);
	PREPARE_VPU_CLK(clk_apu_vcore_adl_cg);
	PREPARE_VPU_CLK(clk_apu_vcore_qos_cg);
	PREPARE_VPU_CLK(clk_apu_conn_apu_cg);
	PREPARE_VPU_CLK(clk_apu_conn_ahb_cg);
	PREPARE_VPU_CLK(clk_apu_conn_axi_cg);
	PREPARE_VPU_CLK(clk_apu_conn_isp_cg);
	PREPARE_VPU_CLK(clk_apu_conn_cam_adl_cg);
	PREPARE_VPU_CLK(clk_apu_conn_img_adl_cg);
	PREPARE_VPU_CLK(clk_apu_conn_emi_26m_cg);
	PREPARE_VPU_CLK(clk_apu_conn_vpu_udi_cg);
	PREPARE_VPU_CLK(clk_apu_core0_jtag_cg);
	PREPARE_VPU_CLK(clk_apu_core0_axi_m_cg);
	PREPARE_VPU_CLK(clk_apu_core0_apu_cg);
	PREPARE_VPU_CLK(clk_apu_core1_jtag_cg);
	PREPARE_VPU_CLK(clk_apu_core1_axi_m_cg);
	PREPARE_VPU_CLK(clk_apu_core1_apu_cg);
	PREPARE_VPU_CLK(clk_top_dsp_sel);
	PREPARE_VPU_CLK(clk_top_dsp1_sel);
	PREPARE_VPU_CLK(clk_top_dsp2_sel);
	PREPARE_VPU_CLK(clk_top_ipu_if_sel);
	PREPARE_VPU_CLK(clk_top_clk26m);
	PREPARE_VPU_CLK(clk_top_univpll_d3_d8);
	PREPARE_VPU_CLK(clk_top_univpll_d3_d4);
	PREPARE_VPU_CLK(clk_top_mainpll_d2_d4);
	PREPARE_VPU_CLK(clk_top_univpll_d3_d2);
	PREPARE_VPU_CLK(clk_top_mainpll_d2_d2);
	PREPARE_VPU_CLK(clk_top_univpll_d2_d2);
	PREPARE_VPU_CLK(clk_top_mainpll_d3);
	PREPARE_VPU_CLK(clk_top_univpll_d3);
	PREPARE_VPU_CLK(clk_top_mmpll_d7);
	PREPARE_VPU_CLK(clk_top_mmpll_d6);
	PREPARE_VPU_CLK(clk_top_adsppll_d5);
	PREPARE_VPU_CLK(clk_top_tvdpll_ck);
	PREPARE_VPU_CLK(clk_top_tvdpll_mainpll_d2_ck);
	PREPARE_VPU_CLK(clk_top_univpll_d2);
	PREPARE_VPU_CLK(clk_top_adsppll_d4);
	PREPARE_VPU_CLK(clk_top_mainpll_d2);
	PREPARE_VPU_CLK(clk_top_mmpll_d4);

#undef PREPARE_VPU_CLK

#endif
	return ret;
}
static int vpu_enable_regulator_and_clock(int core)
{
#ifdef MTK_VPU_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);

	is_power_on[core] = true;
	force_change_vcore_opp[core] = false;
	force_change_vvpu_opp[core] = false;
	force_change_dsp_freq[core] = false;
	return 0;
#else
	int ret = 0;
	int ret1 = 0;
	int get_vcore_opp = 0;
	int get_vvpu_opp = 0;
	//bool adjust_vcore = false;
	bool adjust_vvpu = false;

	LOG_DBG("[vpu_%d] en_rc + (%d)\n", core, is_power_debug_lock);
	vpu_trace_begin("%s", __func__);

	/*--enable regulator--*/
	ret1 = vvpu_regulator_set_mode(true);
	udelay(100);//slew rate:rising10mV/us
	if (g_vpu_log_level > Log_STATE_MACHINE)
		LOG_INF("enable vvpu ret:%d\n", ret1);
	ret1 = vmdla_regulator_set_mode(true);
	udelay(100);//slew rate:rising10mV/us
	if (g_vpu_log_level > Log_STATE_MACHINE)
		LOG_INF("enable vmdla ret:%d\n", ret1);
	vvpu_vmdla_vcore_checker();


	get_vvpu_opp = vpu_get_hw_vvpu_opp(core);
	//if (opps.vvpu.index != get_vvpu_opp)
		adjust_vvpu = true;

	vpu_trace_begin("vcore:request");

	if (adjust_vvpu) {
		LOG_DBG("[vpu_%d] en_rc wait for changing vcore opp", core);
		ret = wait_to_do_change_vcore_opp(core);
		if (ret) {

			/* skip change vcore in these time */
			LOG_WRN("[vpu_%d] timeout to %s(%d/%d), ret=%d\n",
				core,
				"wait_to_do_change_vcore_opp",
				opps.vcore.index,
				get_vcore_opp,
				ret);

			ret = 0;
			goto clk_on;
		}

		LOG_DBG("[vpu_%d] en_rc to do vvpu opp change", core);
#ifdef ENABLE_PMQOS
		switch (opps.vvpu.index) {
		case 0:
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_0);
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_0);
			break;
		case 1:
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_1);
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_2);
				break;
		case 2:
		default:
			mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core],
								VMDLA_OPP_2);
			mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core],
								VVPU_OPP_2);
			mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
								VCORE_OPP_2);

			break;
		}
#else
		ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
							opps.vcore.index);
#endif
	}
	vpu_trace_end();
	if (ret) {
		LOG_ERR("[vpu_%d]fail to request vcore, step=%d\n",
				core, opps.vcore.index);
		goto out;
	}
	LOG_DBG("[vpu_%d] adjust(%d,%d) result vvpu=%d, vmdla=%d\n",
			core,
			adjust_vvpu,
			opps.vvpu.index,
			regulator_get_voltage(vvpu_reg_id),
			regulator_get_voltage(vmdla_reg_id));

	LOG_DBG("[vpu_%d] en_rc setmmdvfs(%d) done\n", core, opps.vcore.index);

clk_on:
#define ENABLE_VPU_MTCMOS(clk) \
{ \
	if (clk != NULL) { \
		if (clk_prepare_enable(clk)) \
			LOG_ERR("fail to prepare&enable mtcmos:%s\n", #clk); \
	} else { \
		LOG_WRN("mtcmos not existed: %s\n", #clk); \
	} \
}

#define ENABLE_VPU_CLK(clk) \
	{ \
		if (clk != NULL) { \
			if (clk_enable(clk)) \
				LOG_ERR("fail to enable clock: %s\n", #clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}


	vpu_trace_begin("clock:enable_source");
	ENABLE_VPU_CLK(clk_top_dsp_sel);
	ENABLE_VPU_CLK(clk_top_ipu_if_sel);
	ENABLE_VPU_CLK(clk_top_dsp1_sel);
	ENABLE_VPU_CLK(clk_top_dsp2_sel);
	vpu_trace_end();

	vpu_trace_begin("mtcmos:enable");
	ENABLE_VPU_MTCMOS(mtcmos_dis);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_vcore_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_conn_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_core0_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_core1_shutdown);

	vpu_trace_end();
	udelay(500);

	vpu_trace_begin("clock:enable");
	ENABLE_VPU_CLK(clk_mmsys_gals_ipu2mm);
	ENABLE_VPU_CLK(clk_mmsys_gals_ipu12mm);
	ENABLE_VPU_CLK(clk_mmsys_gals_comm0);
	ENABLE_VPU_CLK(clk_mmsys_gals_comm1);
	ENABLE_VPU_CLK(clk_mmsys_smi_common);
	/*move vcore cg ctl to atf*/
	vcore_cg_ctl(1);
	ENABLE_VPU_CLK(clk_apu_conn_apu_cg);
	ENABLE_VPU_CLK(clk_apu_conn_ahb_cg);
	ENABLE_VPU_CLK(clk_apu_conn_axi_cg);
	ENABLE_VPU_CLK(clk_apu_conn_isp_cg);
	ENABLE_VPU_CLK(clk_apu_conn_cam_adl_cg);
	ENABLE_VPU_CLK(clk_apu_conn_img_adl_cg);
	ENABLE_VPU_CLK(clk_apu_conn_emi_26m_cg);
	ENABLE_VPU_CLK(clk_apu_conn_vpu_udi_cg);
	switch (core) {
	case 0:
	default:
		ENABLE_VPU_CLK(clk_apu_core0_jtag_cg);
		ENABLE_VPU_CLK(clk_apu_core0_axi_m_cg);
		ENABLE_VPU_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		ENABLE_VPU_CLK(clk_apu_core1_jtag_cg);
		ENABLE_VPU_CLK(clk_apu_core1_axi_m_cg);
		ENABLE_VPU_CLK(clk_apu_core1_apu_cg);
		break;
	}
	vpu_trace_end();

#undef ENABLE_VPU_MTCMOS
#undef ENABLE_VPU_CLK

	LOG_INF("[vpu_%d] en_rc setclksrc(%d/%d/%d/%d)\n",
			core,
			opps.dsp.index,
			opps.dspcore[0].index,
			opps.dspcore[1].index,
			opps.ipu_if.index);

	ret = vpu_if_set_clock_source(clk_top_dsp_sel, opps.dsp.index);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set dsp freq, step=%d, ret=%d\n",
				core, opps.dsp.index, ret);
		goto out;
	}

	ret = vpu_set_clock_source(clk_top_dsp1_sel, opps.dspcore[0].index);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set dsp0 freq, step=%d, ret=%d\n",
				core, opps.dspcore[0].index, ret);
		goto out;
	}

	ret = vpu_set_clock_source(clk_top_dsp2_sel, opps.dspcore[1].index);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set dsp1 freq, step=%d, ret=%d\n",
				core, opps.dspcore[1].index, ret);
		goto out;
	}

	ret = vpu_if_set_clock_source(clk_top_ipu_if_sel, opps.ipu_if.index);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set ipu_if freq, step=%d, ret=%d\n",
				core, opps.ipu_if.index, ret);
		goto out;
	}

out:
	vpu_trace_end();
	if (g_vpu_log_level > Log_STATE_MACHINE)
		apu_get_power_info();
	is_power_on[core] = true;
	force_change_vcore_opp[core] = false;
	force_change_vvpu_opp[core] = false;
	force_change_dsp_freq[core] = false;
	LOG_DBG("[vpu_%d] en_rc -\n", core);
	return ret;
#endif
}
void vpu_enable_mtcmos(void)
{
#define ENABLE_VPU_MTCMOS(clk) \
{ \
	if (clk != NULL) { \
		if (clk_prepare_enable(clk)) \
			LOG_ERR("fail to prepare&enable mtcmos:%s\n", #clk); \
	} else { \
		LOG_WRN("mtcmos not existed: %s\n", #clk); \
	} \
}
	ENABLE_VPU_MTCMOS(mtcmos_dis);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_vcore_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_conn_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_core0_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_core1_shutdown);
	ENABLE_VPU_MTCMOS(mtcmos_vpu_core2_shutdown);

	udelay(500);
	/*move vcore cg ctl to atf*/
	vcore_cg_ctl(1);

#undef ENABLE_VPU_MTCMOS
}
EXPORT_SYMBOL(vpu_enable_mtcmos);

void vpu_disable_mtcmos(void)
{

#define DISABLE_VPU_MTCMOS(clk) \
		{ \
			if (clk != NULL) { \
				clk_disable_unprepare(clk); \
			} else { \
				LOG_WRN("mtcmos not existed: %s\n", #clk); \
			} \
		}
		DISABLE_VPU_MTCMOS(mtcmos_vpu_core2_shutdown);
		DISABLE_VPU_MTCMOS(mtcmos_vpu_core1_shutdown);
		DISABLE_VPU_MTCMOS(mtcmos_vpu_core0_shutdown);
		DISABLE_VPU_MTCMOS(mtcmos_vpu_conn_shutdown);
		DISABLE_VPU_MTCMOS(mtcmos_vpu_vcore_shutdown);
		DISABLE_VPU_MTCMOS(mtcmos_dis);

#undef DISABLE_VPU_MTCMOS
}
EXPORT_SYMBOL(vpu_disable_mtcmos);

#ifdef MTK_VPU_SMI_DEBUG_ON
static unsigned int vpu_read_smi_bus_debug(int core)
{
	unsigned int smi_bus_value = 0x0;
	unsigned int smi_bus_vpu_value = 0x0;


	if ((int)(vpu_dev->smi_cmn_base) != 0) {
		switch (core) {
		case 0:
		default:
			smi_bus_value =
				vpu_read_reg32(vpu_dev->smi_cmn_base,
							0x414);
			break;
		case 1:
			smi_bus_value =
				vpu_read_reg32(vpu_dev->smi_cmn_base,
							0x418);
			break;
		}
		smi_bus_vpu_value = (smi_bus_value & 0x007FE000) >> 13;
	} else {
		LOG_INF("[vpu_%d] null smi_cmn_base\n", core);
	}
	LOG_INF("[vpu_%d] read_smi_bus (0x%x/0x%x)\n", core,
			smi_bus_value, smi_bus_vpu_value);

	return smi_bus_vpu_value;
}
#endif

static int vpu_disable_regulator_and_clock(int core)
{
	int ret = 0;
	int ret1 = 0;

#ifdef MTK_VPU_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);

	is_power_on[core] = false;
	if (!is_power_debug_lock)
		opps.dspcore[core].index = 7;

	return ret;
#else
	unsigned int smi_bus_vpu_value = 0x0;

	/* check there is un-finished transaction in bus before
	 * turning off vpu power
	 */
#ifdef MTK_VPU_SMI_DEBUG_ON
	smi_bus_vpu_value = vpu_read_smi_bus_debug(core);

	LOG_INF("[vpu_%d] dis_rc 1 (0x%x)\n", core, smi_bus_vpu_value);

	if ((int)smi_bus_vpu_value != 0) {
		mdelay(1);
		smi_bus_vpu_value = vpu_read_smi_bus_debug(core);

		LOG_INF("[vpu_%d] dis_rc again (0x%x)\n", core,
				smi_bus_vpu_value);

		if ((int)smi_bus_vpu_value != 0) {
			smi_debug_bus_hanging_detect_ext2(0x1ff, 1, 0, 1);
			vpu_aee_warn("VPU SMI CHECK",
				"core_%d fail to check smi, value=%d\n",
				core,
				smi_bus_vpu_value);
		}
	}
#else
	LOG_DVFS("[vpu_%d] dis_rc + (0x%x)\n", core, smi_bus_vpu_value);
#endif

#define DISABLE_VPU_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable(clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}
	switch (core) {
	case 0:
	default:
		DISABLE_VPU_CLK(clk_apu_core0_jtag_cg);
		DISABLE_VPU_CLK(clk_apu_core0_axi_m_cg);
		DISABLE_VPU_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		DISABLE_VPU_CLK(clk_apu_core1_jtag_cg);
		DISABLE_VPU_CLK(clk_apu_core1_axi_m_cg);
		DISABLE_VPU_CLK(clk_apu_core1_apu_cg);
		break;
	}

	DISABLE_VPU_CLK(clk_apu_conn_apu_cg);
	DISABLE_VPU_CLK(clk_apu_conn_ahb_cg);
	DISABLE_VPU_CLK(clk_apu_conn_axi_cg);
	DISABLE_VPU_CLK(clk_apu_conn_isp_cg);
	DISABLE_VPU_CLK(clk_apu_conn_cam_adl_cg);
	DISABLE_VPU_CLK(clk_apu_conn_img_adl_cg);
	DISABLE_VPU_CLK(clk_apu_conn_emi_26m_cg);
	DISABLE_VPU_CLK(clk_apu_conn_vpu_udi_cg);
	DISABLE_VPU_CLK(clk_mmsys_gals_ipu2mm);
	DISABLE_VPU_CLK(clk_mmsys_gals_ipu12mm);
	DISABLE_VPU_CLK(clk_mmsys_gals_comm0);
	DISABLE_VPU_CLK(clk_mmsys_gals_comm1);
	DISABLE_VPU_CLK(clk_mmsys_smi_common);
	LOG_DBG("[vpu_%d] dis_rc flag4\n", core);

#define DISABLE_VPU_MTCMOS(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable_unprepare(clk); \
		} else { \
			LOG_WRN("mtcmos not existed: %s\n", #clk); \
		} \
	}
	DISABLE_VPU_MTCMOS(mtcmos_vpu_core1_shutdown);
	DISABLE_VPU_MTCMOS(mtcmos_vpu_core0_shutdown);
	DISABLE_VPU_MTCMOS(mtcmos_vpu_conn_shutdown);
	DISABLE_VPU_MTCMOS(mtcmos_vpu_vcore_shutdown);
	DISABLE_VPU_MTCMOS(mtcmos_dis);


	DISABLE_VPU_CLK(clk_top_dsp_sel);
	DISABLE_VPU_CLK(clk_top_ipu_if_sel);
	DISABLE_VPU_CLK(clk_top_dsp1_sel);
	DISABLE_VPU_CLK(clk_top_dsp2_sel);

#undef DISABLE_VPU_MTCMOS
#undef DISABLE_VPU_CLK
#ifdef ENABLE_PMQOS
	LOG_DVFS("[vpu_%d]pc0=%d, pc1=%d\n",
		core, power_counter[0], power_counter[1]);
	mtk_pm_qos_update_request(&vpu_qos_vmdla_request[core], VMDLA_OPP_2);
	mtk_pm_qos_update_request(&vpu_qos_vvpu_request[core], VVPU_OPP_2);
	mtk_pm_qos_update_request(&vpu_qos_vcore_request[core],
							VCORE_OPP_UNREQ);
#else
	ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
						MMDVFS_FINE_STEP_UNREQUEST);
#endif
	if (ret) {
		LOG_ERR("[vpu_%d]fail to unrequest vcore!\n", core);
		goto out;
	}
	LOG_DBG("[vpu_%d] disable result vvpu=%d\n",
		core, regulator_get_voltage(vvpu_reg_id));
out:

	/*--disable regulator--*/
	ret1 = vmdla_regulator_set_mode(false);
	udelay(100);//slew rate:rising10mV/us
	LOG_DBG("disable vmdla ret:%d\n", ret1);
	ret1 = vvpu_regulator_set_mode(false);
	udelay(100);//slew rate:rising10mV/us
	LOG_DBG("disable vvpu ret:%d\n", ret1);
	vvpu_vmdla_vcore_checker();
	is_power_on[core] = false;
	if (!is_power_debug_lock) {
		opps.dspcore[core].index = 15;
		opps.dsp.index = 9;
		opps.ipu_if.index = 9;
	}
if (g_vpu_log_level > Log_STATE_MACHINE)
	LOG_INF("[vpu_%d] dis_rc -\n", core);
	return ret;
#endif
}

static void vpu_unprepare_regulator_and_clock(void)
{

#ifdef MTK_VPU_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);
#else
#define UNPREPARE_VPU_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_unprepare(clk); \
			clk = NULL; \
		} \
	}
	UNPREPARE_VPU_CLK(clk_apu_core0_jtag_cg);
	UNPREPARE_VPU_CLK(clk_apu_core0_axi_m_cg);
	UNPREPARE_VPU_CLK(clk_apu_core0_apu_cg);
	UNPREPARE_VPU_CLK(clk_apu_core1_jtag_cg);
	UNPREPARE_VPU_CLK(clk_apu_core1_axi_m_cg);
	UNPREPARE_VPU_CLK(clk_apu_core1_apu_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_apu_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_ahb_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_axi_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_isp_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_cam_adl_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_img_adl_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_emi_26m_cg);
	UNPREPARE_VPU_CLK(clk_apu_conn_vpu_udi_cg);
	UNPREPARE_VPU_CLK(clk_apu_vcore_ahb_cg);
	UNPREPARE_VPU_CLK(clk_apu_vcore_axi_cg);
	UNPREPARE_VPU_CLK(clk_apu_vcore_adl_cg);
	UNPREPARE_VPU_CLK(clk_apu_vcore_qos_cg);
	UNPREPARE_VPU_CLK(clk_mmsys_gals_ipu2mm);
	UNPREPARE_VPU_CLK(clk_mmsys_gals_ipu12mm);
	UNPREPARE_VPU_CLK(clk_mmsys_gals_comm0);
	UNPREPARE_VPU_CLK(clk_mmsys_gals_comm1);
	UNPREPARE_VPU_CLK(clk_mmsys_smi_common);
	UNPREPARE_VPU_CLK(clk_top_dsp_sel);
	UNPREPARE_VPU_CLK(clk_top_dsp1_sel);
	UNPREPARE_VPU_CLK(clk_top_dsp2_sel);
	UNPREPARE_VPU_CLK(clk_top_ipu_if_sel);
	UNPREPARE_VPU_CLK(clk_top_clk26m);
	UNPREPARE_VPU_CLK(clk_top_univpll_d3_d8);
	UNPREPARE_VPU_CLK(clk_top_univpll_d3_d4);
	UNPREPARE_VPU_CLK(clk_top_mainpll_d2_d4);
	UNPREPARE_VPU_CLK(clk_top_univpll_d3_d2);
	UNPREPARE_VPU_CLK(clk_top_mainpll_d2_d2);
	UNPREPARE_VPU_CLK(clk_top_univpll_d2_d2);
	UNPREPARE_VPU_CLK(clk_top_mainpll_d3);
	UNPREPARE_VPU_CLK(clk_top_univpll_d3);
	UNPREPARE_VPU_CLK(clk_top_mmpll_d7);
	UNPREPARE_VPU_CLK(clk_top_mmpll_d6);
	UNPREPARE_VPU_CLK(clk_top_adsppll_d5);
	UNPREPARE_VPU_CLK(clk_top_tvdpll_ck);
	UNPREPARE_VPU_CLK(clk_top_tvdpll_mainpll_d2_ck);
	UNPREPARE_VPU_CLK(clk_top_univpll_d2);
	UNPREPARE_VPU_CLK(clk_top_adsppll_d4);
	UNPREPARE_VPU_CLK(clk_top_mainpll_d2);
	UNPREPARE_VPU_CLK(clk_top_mmpll_d4);



#undef UNPREPARE_VPU_CLK
#endif
}

#define MET_VPU_LOG
#ifdef MET_VPU_LOG
/* log format */
#define VPUBUF_MARKER		(0xBEEF)
#define VPULOG_START_MARKER	(0x55AA)
#define VPULOG_END_MARKER	(0xAA55)
#define MX_LEN_STR_DESC		(128)

#pragma pack(push)
#pragma pack(2)
struct vpulog_format_head_t {
	unsigned short start_mark;
	unsigned char desc_len;
	unsigned char action_id;
	unsigned int sys_timer_h;
	unsigned int sys_timer_l;
	unsigned short sessid;
};
#pragma pack(pop)

struct vpu_log_reader_t {
	struct list_head list;
	int core;
	unsigned int buf_addr;
	unsigned int buf_size;
	void *ptr;
};

struct my_ftworkQ_struct_t {
	struct list_head list;
	spinlock_t my_lock;
	int pid;
	struct work_struct my_work;
};

static struct my_ftworkQ_struct_t ftrace_dump_work[MTK_VPU_CORE];
static void vpu_dump_ftrace_workqueue(struct work_struct *);
static int vpu_check_postcond(int core);

static void __MET_PACKET__(int vpu_core_s, unsigned long long wclk,
	 unsigned char action_id, char *str_desc, unsigned int sessid)
{
	char action = 'Z';
	char null_str[] = "null";
	char *__str_desc = str_desc;
	int val = 0;
	unsigned int vpu_core = (unsigned int)vpu_core_s;

	switch (action_id) {
	/* For Sync Maker Begin/End */
	case 0x01:
		action = 'B';
		break;
	case 0x02:
		action = 'E';
		break;
	/* For Counter Maker */
	case 0x03:
		action = 'C';
		/* counter type: */
		/* bit 0~11: string desc */
		/* bit 12~15: count val */
		val = *(unsigned int *)(str_desc + 12);
		break;
	/* for Async Marker Start/Finish */
	case 0x04:
		action = 'S';
		break;
	case 0x05:
		action = 'F';
		break;
	}
	if (str_desc[0] == '\0') {
		/* null string handle */
		__str_desc =  null_str;
	}

	vpu_met_packet(wclk, action, vpu_core, ftrace_dump_work[vpu_core].pid,
		sessid + 0x8000 * vpu_core, __str_desc, val);
}

static void dump_buf(void *ptr, int leng)
{
	int idx = 0;
	unsigned short *usaddr;

	for (idx = 0; idx < leng; idx += 16) {
		usaddr = (unsigned short *)(ptr + idx);
		vpu_trace_dump("%08x: %04x%04x %04x%04x %04x%04x %04x%04x,",
			idx,
			usaddr[0], usaddr[1], usaddr[2], usaddr[3],
			usaddr[4], usaddr[5], usaddr[6], usaddr[7]
		);
	}

}

static int vpulog_clone_buffer(int core_s, unsigned int addr,
	unsigned int size, void *ptr)
{
	int idx = 0;
	unsigned int core = (unsigned int)core_s;

	for (idx = 0; idx < size; idx += 4) {
		/* read 4 bytes from VPU DMEM */
		*((unsigned int *)(ptr + idx)) =
		vpu_read_reg32(vpu_service_cores[core].vpu_base, addr + idx);
	}
	/* notify VPU buffer copy is finish */
	vpu_write_field(core, FLD_XTENSA_INFO18, 0x00000000);
	return 0;
}

static bool vpulog_check_buff_ready(void *buf, int buf_len)
{
	if (VPUBUF_MARKER != *(unsigned short *)buf) {
		/* front marker is invalid*/
		vpu_trace_dump("Error front maker: %04x",
		*(unsigned short *)buf);
		return false;
	}
	if (VPUBUF_MARKER != *(unsigned short *)(buf + buf_len - 2)) {
		vpu_trace_dump("Error end maker: %04x",
			*(unsigned short *)(buf + buf_len - 2));
		return false;
	}
	return true;
}

static bool vpulog_check_packet_valid(struct vpulog_format_head_t *lft)
{
	if (lft->start_mark != VPULOG_START_MARKER) {
		vpu_trace_dump("Error lft->start_mark: %02x", lft->start_mark);
		return false;
	}
	if (lft->desc_len > 0x10) {
		vpu_trace_dump("Error lft->desc_len: %02x", lft->desc_len);
		return false;
	}
	return true;
}

static void vpu_log_parser(int vpu_core, void *ptr, int buf_leng)
{
	int idx = 0;
	void *start_ptr = ptr;
	int valid_len = 0;
	struct vpulog_format_head_t *lft;
	char trace_data[MX_LEN_STR_DESC];
	int vpulog_format_header_size = sizeof(struct vpulog_format_head_t);

	/*check buffer status*/
	if (false == vpulog_check_buff_ready(start_ptr, buf_leng)) {
		vpu_trace_dump("vpulog_check_buff_ready fail: %p %d",
			ptr, buf_leng);
		dump_buf(start_ptr, buf_leng);
		return;
	}

	/* get valid length*/
	valid_len = *(unsigned short *)(start_ptr + 2);
	if (valid_len >= buf_leng) {
		vpu_trace_dump("valid_len: %d large than buf_leng: %d",
			valid_len, buf_leng);
		return;
	}
	/*data offset start after Marker,Length*/
	idx += 4;

	if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_begin("%s|vpu%d|@%p/%d",
		__func__, vpu_core, ptr, buf_leng);
	while (1) {
		unsigned long long sys_t;
		int packet_size = 0;
		void *data_ptr;

		if (idx >= valid_len)
			break;

		lft = (struct vpulog_format_head_t *)(start_ptr + idx);
		data_ptr = (start_ptr + idx) + vpulog_format_header_size;
		if (false == vpulog_check_packet_valid(lft)) {
			vpu_trace_dump("vpulog_check_packet_valid fail");
			dump_buf(start_ptr, buf_leng);
			break;
		}

		/*calculate packet size: header + data + end_mark*/
		packet_size = vpulog_format_header_size + lft->desc_len + 2;
		if (idx + packet_size > valid_len) {
			vpu_trace_dump(
				"error length (idx: %d, packet_size: %d)",
				idx, packet_size);
			vpu_trace_dump("out of bound: valid_len: %d",
			valid_len);
			dump_buf(start_ptr, buf_leng);
			break;
		}

		if (lft->desc_len > MX_LEN_STR_DESC) {
			vpu_trace_dump(
				"lft->desc_len(%d) >	MX_LEN_STR_DESC(%d)",
				lft->desc_len, MX_LEN_STR_DESC);
			dump_buf(start_ptr, buf_leng);
			break;
		}
		memset(trace_data, 0x00, MX_LEN_STR_DESC);
		if (lft->desc_len > 0) {
			/*copy data buffer*/
			memcpy(trace_data, data_ptr, lft->desc_len);
		}
		sys_t = lft->sys_timer_h;
		sys_t = (sys_t << 32) + (lft->sys_timer_l & 0xFFFFFFFF);

		__MET_PACKET__(
			vpu_core,
			sys_t,
			lft->action_id,
			trace_data,
			lft->sessid
		);
		idx += packet_size;
	}
	if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_end();
}
static unsigned int addr_tran_vpu2apmcu(int core, unsigned int buf_addr)
{
	unsigned int apmcu_log_buf = 0x0;
	unsigned int apmcu_log_buf_ofst = 0xFFFFFFFF;

	switch (core) {
	case 0:
		apmcu_log_buf = ((buf_addr & 0x000fffff) | 0x19100000);
		apmcu_log_buf_ofst = apmcu_log_buf - 0x19100000;
		break;
	case 1:
		apmcu_log_buf = ((buf_addr & 0x000fffff) | 0x19200000);
		apmcu_log_buf_ofst = apmcu_log_buf - 0x19200000;
		break;
	default:
		LOG_ERR("wrong core(%d)\n", core);
		goto out;
	}
out:
	return apmcu_log_buf_ofst;
}
static void vpu_dump_ftrace_workqueue(struct work_struct *work)
{
	unsigned int log_buf_size = 0x0;
	struct my_ftworkQ_struct_t *my_work_core = container_of(work,
		struct my_ftworkQ_struct_t, my_work);
	struct list_head *entry;
	struct vpu_log_reader_t *vlr;
	void *ptr;
	int vpu_core;

	if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_begin("VPU_LOG_ISR_BOTTOM_HALF");
	/* protect area start */
list_rescan:
	spin_lock_irq(&(my_work_core->my_lock));
	list_for_each(entry, &(my_work_core->list)) {
		vlr = list_entry(entry, struct vpu_log_reader_t, list);
		if (vlr != NULL) {
			log_buf_size = vlr->buf_size;
			vpu_core = vlr->core;
			vpu_trace_dump("%s %d addr/size/ptr: %08x/%08x/%p",
				__func__, __LINE__, vlr->buf_addr,
				vlr->buf_size, vlr->ptr);
			ptr = vlr->ptr;
			if (ptr) {
				vpu_log_parser(vpu_core, ptr, log_buf_size);
				kfree(ptr);
				vlr->ptr = NULL;
			} else {
				vpu_trace_dump("my_work_core->ptr is NULL");
			}
			list_del(entry);
			kfree(vlr);
		} else {
			vpu_trace_dump("%s %d vlr is null",
				__func__, __LINE__);
			list_del(entry);
		}
		spin_unlock_irq(&(my_work_core->my_lock));
		goto list_rescan;
	}
	spin_unlock_irq(&(my_work_core->my_lock));
	/* protect area end */
	if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
		vpu_trace_end();
}

#define VPU_MOVE_WAKE_TO_BACK
static int isr_common_handler(int core_s)
{
	int req_cmd = 0, normal_check_done = 0;
	int req_dump = 0;
	unsigned int apmcu_log_buf_ofst;
	unsigned int log_buf_addr = 0x0;
	unsigned int log_buf_size = 0x0;
	struct vpu_log_reader_t *vpu_log_reader;
	void *ptr;
	uint32_t status;
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("vpu %d received a interrupt\n", core);

	/* INFO 17 was used to reply command done */
	req_cmd = vpu_read_field(core, FLD_XTENSA_INFO17);
	LOG_DBG("INFO17=0x%08x\n", req_cmd);
	vpu_trace_dump("VPU%d_ISR_RECV|INFO17=0x%08x", core, req_cmd);
	switch (req_cmd) {
	case 0:
		break;
	case VPU_REQ_DO_CHECK_STATE:
	default:
		if (vpu_check_postcond(core) == -EBUSY) {
		/* host may receive isr to dump */
		/* ftrace log while d2d is stilling running */
		/* but the info17 is set as 0x100 */
		/* in this case, we do nothing for */
		/* cmd state control */
		/* flow while device status is busy */
			vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE BUSY",
			core);
		} else {
			/* other normal cases for cmd state control flow */
			normal_check_done = 1;
#ifndef VPU_MOVE_WAKE_TO_BACK
			vpu_service_cores[core].is_cmd_done = true;
			wake_up_interruptible(&cmd_wait);
			vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE OK", core);
#endif
		}
		break;
	}


	/* INFO18 was used to dump MET Log */
	req_dump = vpu_read_field(core, FLD_XTENSA_INFO18);
	LOG_DBG("INFO18=0x%08x\n", req_dump);
	vpu_trace_dump("VPU%d_ISR_RECV|INFO18=0x%08x", core, req_dump);
	/* dispatch interrupt by INFO18 */
	switch (req_dump) {
	case 0:
		break;
	case VPU_REQ_DO_DUMP_LOG:
		/* handle output log */
		if (g_func_mask & VFM_ROUTINE_PRT_SYSLOG) {
			log_buf_addr = vpu_read_field(core, FLD_XTENSA_INFO05);
			log_buf_size = vpu_read_field(core, FLD_XTENSA_INFO06);

			/* translate vpu address to apmcu */
			apmcu_log_buf_ofst =
			addr_tran_vpu2apmcu(core, log_buf_addr);
			if (g_vpu_log_level > VpuLogThre_PERFORMANCE) {
				vpu_trace_dump(
	"[vpu_%d] log buf/size(0x%08x/0x%08x)-> log_apmcu offset(0x%08x)",
				core,
				log_buf_addr,
				log_buf_size,
				apmcu_log_buf_ofst);
			}
			if (apmcu_log_buf_ofst == 0xFFFFFFFF) {
				LOG_ERR(
		"addr_tran_vpu2apmcu translate fail!\n");
				goto info18_out;
			}
			/* in ISR we need use ATOMIC flag to alloc memory */
			ptr = kmalloc(log_buf_size, GFP_ATOMIC);
			if (ptr) {
				if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
					vpu_trace_begin(
		"VPULOG_ISR_TOPHALF|VPU%d"
					, core);
		vpu_log_reader =
		kmalloc(sizeof(struct vpu_log_reader_t), GFP_ATOMIC);
		if (vpu_log_reader) {
			/* fill vpu_log reader's information */
			vpu_log_reader->core = core;
			vpu_log_reader->buf_addr = log_buf_addr;
			vpu_log_reader->buf_size = log_buf_size;
			vpu_log_reader->ptr = ptr;

			LOG_DBG("%s %d [vpu_%d] addr/size/ptr: %08x/%08x/%p\n",
			__func__, __LINE__,
			core,
			log_buf_addr,
			log_buf_size,
			ptr);
		/* clone buffer in isr*/
		if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
			vpu_trace_begin("VPULOG_CLONE_BUFFER|VPU%d|%08x/%u->%p",
				core,
				log_buf_addr,
				log_buf_size,
				ptr);
		vpulog_clone_buffer(core,
			apmcu_log_buf_ofst, log_buf_size, ptr);
		if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
			vpu_trace_end();
		/* dump_buf(ptr, log_buf_size); */

		/* protect area start */
		spin_lock(&(ftrace_dump_work[core].my_lock));
		list_add_tail(&(vpu_log_reader->list),
			&(ftrace_dump_work[core].list));
		spin_unlock(&(ftrace_dump_work[core].my_lock));
		/* protect area end */

		/* dump log to ftrace on BottomHalf */
		schedule_work(&(ftrace_dump_work[core].my_work));
			} else {
				LOG_ERR("vpu_log_reader alloc fail");
				kfree(ptr);
			}
			if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
				vpu_trace_end();
			} else {
				LOG_ERR("vpu buffer alloc fail");
			}
		}
		break;
	case VPU_REQ_DO_CLOSED_FILE:
		break;
	default:
		LOG_ERR("vpu %d not support cmd..(%d)\n", core,
			req_dump);
		break;
	}

info18_out:
	/* clear int */
	status = vpu_read_field(core, FLD_APMCU_INT);

	if (status != 0) {
		LOG_ERR("vpu %d FLD_APMCU_INT = (%d)\n", core,
			status);
	}
	vpu_write_field(core, FLD_APMCU_INT, 1);

#ifdef VPU_MOVE_WAKE_TO_BACK
	if (normal_check_done == 1) {
		vpu_trace_dump("VPU%d VPU_REQ_DO_CHECK_STATE OK", core);
		LOG_INF("normal_check_done UNLOCK\n");
		vpu_service_cores[core].is_cmd_done = true;
		LOG_INF("%s: vpu%d: done:%d, info00:%xh, info17:%xh, pc:%0xh\n",
			__func__, core,
			vpu_service_cores[core].is_cmd_done,
			vpu_read_field(core, FLD_XTENSA_INFO00),
			req_cmd,
			vpu_read_field(core, FLD_P_DEBUG_PC));
		wake_up_interruptible(&cmd_wait);
	}
#endif
	return IRQ_HANDLED;
}

irqreturn_t vpu0_isr_handler(int irq, void *dev_id)
{
	return isr_common_handler(0);
}

irqreturn_t vpu1_isr_handler(int irq, void *dev_id)
{
	return isr_common_handler(1);
}
#else
irqreturn_t vpu0_isr_handler(int irq, void *dev_id)
{
	LOG_DBG("vpu 0 received a interrupt\n");
	vpu_service_cores[0].is_cmd_done = true;
	wake_up_interruptible(&cmd_wait);
	vpu_write_field(0, FLD_APMCU_INT, 1);                   /* clear int */

	return IRQ_HANDLED;
}
irqreturn_t vpu1_isr_handler(int irq, void *dev_id)
{
	LOG_DBG("vpu 1 received a interrupt\n");
	vpu_service_cores[1].is_cmd_done = true;
	wake_up_interruptible(&cmd_wait);
	vpu_write_field(1, FLD_APMCU_INT, 1);                   /* clear int */

	return IRQ_HANDLED;
}
#endif

static int pools_are_empty(int core)
{
	return (vpu_pool_is_empty(&vpu_dev->pool[core]) &&
		vpu_pool_is_empty(&vpu_dev->pool_common) &&
		vpu_pool_is_empty(&vpu_dev->pool_multiproc));
}

static void vpu_hw_ion_free_handle(struct ion_client *client,
	struct ion_handle *handle)
{
	LOG_DBG("[vpu] ion_free_handle +\n");

	if (!client) {
		LOG_WRN("[vpu] invalid ion client(0x%p)!\n", client);
		return;
	}
	if (!handle) {
		LOG_WRN("[vpu] invalid ion handle(0x%p)!\n", handle);
		return;
	}
	if (g_vpu_log_level > Log_STATE_MACHINE)
		LOG_INF("[vpu] ion_free_handle(0x%p)\n", handle);

	ion_free(client, handle);
}

static int vpu_service_routine(void *arg)
{
	struct vpu_user *user = NULL;
	struct vpu_request *req = NULL;
	/*struct vpu_algo *algo = NULL;*/
	uint8_t vcore_opp_index = 0xFF;
	uint8_t vvpu_opp_index = 0xFF;
	uint8_t dsp_freq_index = 0xFF;
	struct vpu_user *user_in_list = NULL;
	struct list_head *head = NULL;
	int *d = (int *)arg;
	unsigned int service_core = (*d);
	bool get = false;
	int i = 0, j = 0, cnt = 0;

	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	for (; !kthread_should_stop();) {
		/* wait for requests if there is no one in user's queue */
		add_wait_queue(&vpu_dev->req_wait, &wait);
		while (1) {
			if (!pools_are_empty(service_core))
				break;
			wait_woken(&wait, TASK_INTERRUPTIBLE,
						MAX_SCHEDULE_TIMEOUT);
		}
		remove_wait_queue(&vpu_dev->req_wait, &wait);

		/* this thread will be stopped if start direct link */
		/* todo, no need in this currently */
		/*wait_event_interruptible(lock_wait, !is_locked);*/

		/* consume the user's queue */
		req = NULL;
		user_in_list = NULL;
		head = NULL;
		get = false;
		cnt = 0;

		/* 1. multi-process pool */
		req = vpu_pool_dequeue(&vpu_dev->pool_multiproc, NULL);

		/* 2. self pool */
		if (!req)
			req = vpu_pool_dequeue(
				&vpu_dev->pool[service_core],
				&vpu_dev->priority_list[service_core][i]);

		/* 3. common pool */
		if (!req)
			req = vpu_pool_dequeue(&vpu_dev->pool_common, NULL);

		/* suppose that req is null would not happen
		 * due to we check service_pool_is_empty and
		 * common_pool_is_empty
		 */
		if (req != NULL) {
			LOG_DBG("[vpu] service core index...: %d/%d\n",
					service_core, (*d));

			user = (struct vpu_user *)req->user_id;

			LOG_DBG("[vpu_%d] user...0x%lx/0x%lx/0x%lx/0x%lx\n",
				service_core,
				(unsigned long)user,
				(unsigned long)&user,
				(unsigned long)req->user_id,
				(unsigned long)&(req->user_id));

			mutex_lock(&vpu_dev->user_mutex);

			/* check to avoid user had been removed from list,
			 * and kernel vpu thread still do the request
			 */
			list_for_each(head, &vpu_dev->user_list)
			{
				user_in_list = vlist_node_of(head,
							struct vpu_user);

				LOG_DBG("[vpu_%d] user->id = 0x%lx, 0x%lx\n",
					service_core,
					(unsigned long)(user_in_list->id),
					(unsigned long)(user));

				if ((unsigned long)(user_in_list->id) ==
							(unsigned long)(user)) {
					get = true;
					LOG_DBG("[vpu_%d] get_0x%lx = true\n",
							service_core,
							(unsigned long)(user));
					break;
				}
			}
			if (!get) {
				mutex_unlock(&vpu_dev->user_mutex);

				LOG_WRN("[vpu_%d] %s(0x%lx) %s\n",
					service_core,
					"get request that the original user",
					(unsigned long)(user),
					"is deleted");

#define ION_FREE_HANDLE \
{ \
for (j = 0 ; j < req->buffers[i].plane_count ; j++) { \
	vpu_hw_ion_free_handle(my_ion_client, \
		(struct ion_handle *)((uintptr_t)(req->buf_ion_infos[cnt]))); \
	cnt++; \
} \
}
				/* release buf ref cnt if user is deleted*/
				for (i = 0 ; i < req->buffer_count ; i++)
					ION_FREE_HANDLE

#undef ION_FREE_HANDLE

				vpu_hw_ion_free_handle(my_ion_client,
					(struct ion_handle *)
					((uintptr_t)(req->sett.sett_ion_fd)));

				continue;
			}

			mutex_lock(&user->data_mutex);
			user->running[service_core] = true; /* */
			mutex_unlock(&user->data_mutex);

			/* unlock for avoiding long time locking */
			mutex_unlock(&vpu_dev->user_mutex);
			if (req->power_param.opp_step == 0xFF) {
				vcore_opp_index = 0xFF;
				vvpu_opp_index = 0xFF;
				dsp_freq_index = 0xFF;
			} else {
				vcore_opp_index =
				 opps.vcore.opp_map[req->power_param.opp_step];
				vvpu_opp_index =
				 opps.vvpu.opp_map[req->power_param.opp_step];

#define TEMP_VAR (opps.dspcore[service_core].opp_map[req->power_param.opp_step])

				dsp_freq_index = TEMP_VAR;

#undef TEMP_VAR

/*if running req priority < priority list, increase opp*/
		LOG_DBG("[vpu_%d] req->priority:%d\n",
		service_core, req->priority);
	for (i = 0 ; i < req->priority ; i++) {
		LOG_DBG("[vpu_%d] priority_list num[%d]:%d\n",
			service_core, i,
				vpu_dev->priority_list[service_core][i]);
		if (vpu_dev->priority_list[service_core][i] > 0) {
			LOG_INF("+ opp due to priority %d\n", req->priority);
			vcore_opp_index = 0;
			dsp_freq_index = 0;
			break;
			}
		}
			}
			LOG_DBG("[vpu_%d] run, opp(%d/%d/%d)\n",
					service_core,
					req->power_param.opp_step,
					vvpu_opp_index,
					dsp_freq_index);

			vpu_opp_check(service_core,
						vvpu_opp_index,
						dsp_freq_index);

#define LOG_STRING \
"[v%d<-0x%x]0x%lx,ID=0x%lx_%d,%d->%d,%d,%d/%d-%d,%d-%d,0x%x,%d/%d/%d/0x%x\n"

			LOG_INF(LOG_STRING,
				service_core,
				req->requested_core,
				(unsigned long)req->user_id,
				(unsigned long)req->request_id,
				req->frame_magic,
				vpu_service_cores[service_core].current_algo,
				(int)(req->algo_id[service_core]),
				req->power_param.opp_step,
				req->power_param.freq_step,
				vvpu_opp_index,
				opps.vcore.index,
				dsp_freq_index,
				opps.dspcore[service_core].index,
				g_func_mask,
				vpu_dev->pool[service_core].size,
				vpu_dev->pool_common.size,
				is_locked,
				efuse_data);

#undef LOG_STRING

			#ifdef CONFIG_PM_SLEEP
			__pm_stay_awake(vpu_wake_lock[service_core]);
			#endif
			exception_isr_check[service_core] = true;
			if (vpu_hw_processing_request(service_core, req)) {
				LOG_ERR("[vpu_%d] %s failed @ Q\n",
					service_core,
					"hw_processing_request");
				req->status = VPU_REQ_STATUS_FAILURE;
				goto out;
			} else {
				req->status = VPU_REQ_STATUS_SUCCESS;
			}
			LOG_DBG("[vpu] flag - 5: hw enque_request done\n");
		} else {
			/* consider that only one req in common pool and all
			 * services get pass through.
			 * do nothing if the service do not get the request
			 */
			LOG_WRN("[vpu_%d] get null request, %d/%d/%d\n",
				service_core,
				vpu_dev->pool[service_core].size,
				vpu_dev->pool_common.size, is_locked);
			continue;
		}
out:
		cnt = 0;
		for (i = 0 ; i < req->buffer_count ; i++) {
			for (j = 0 ; j < req->buffers[i].plane_count ; j++) {
				vpu_hw_ion_free_handle(my_ion_client,
				    (struct ion_handle *)
				       ((uintptr_t)(req->buf_ion_infos[cnt])));
				cnt++;
			}
		}
		vpu_hw_ion_free_handle(my_ion_client,
			(struct ion_handle *)
			   ((uintptr_t)(req->sett.sett_ion_fd)));

		/* if req is null, we should not do anything of
		 * following codes
		 */
		mutex_lock(&(vpu_service_cores[service_core].state_mutex));
		if (vpu_service_cores[service_core].state != VCT_SHUTDOWN)
			vpu_service_cores[service_core].state = VCT_IDLE;
		mutex_unlock(&(vpu_service_cores[service_core].state_mutex));
		#ifdef CONFIG_PM_SLEEP
		__pm_relax(vpu_wake_lock[service_core]);
		#endif
		mutex_lock(&vpu_dev->user_mutex);

		LOG_DBG("[vpu] flag - 5.5 : ....\n");
		/* check to avoid user had been removed from list,
		 * and kernel vpu thread finish the task
		 */
		get = false;
		head = NULL;
		user_in_list = NULL;
		list_for_each(head, &vpu_dev->user_list)
		{
			user_in_list = vlist_node_of(head, struct vpu_user);
			if ((unsigned long)(user_in_list->id) ==
							(unsigned long)(user)) {
				get = true;
				break;
			}
		}

		if (get) {
			mutex_lock(&user->data_mutex);

			LOG_DBG("[vpu] flag - 6: add to deque list\n");

			req->occupied_core = (0x1 << service_core);
			list_add_tail(vlist_link(req, struct vpu_request),
						&user->deque_list);

			LOG_INF("[vpu_%d, 0x%x->0x%x] %s(%d_%d), st(%d) %s\n",
				service_core,
				req->requested_core, req->occupied_core,
				"algo_id", (int)(req->algo_id[service_core]),
				req->frame_magic,
				req->status,
				"add to deque list done");

			user->running[service_core] = false;
			mutex_unlock(&user->data_mutex);
			wake_up_interruptible_all(&user->deque_wait);
			wake_up_interruptible_all(&user->delete_wait);
		} else {
			LOG_WRN("[vpu_%d]%s(0x%lx) is deleted\n",
				service_core,
				"done request that the original user",
				(unsigned long)(user));
		}
		mutex_unlock(&vpu_dev->user_mutex);
		wake_up_interruptible(&waitq_change_vcore);
		/* leave loop of round-robin */
		if (is_locked)
			break;
		/* release cpu for another operations */
		usleep_range(1, 10);
	}
	return 0;
}


#ifndef MTK_VPU_EMULATOR

static int vpu_map_mva_of_bin(int core_s, uint64_t bin_pa)
{
	int ret = 0;
	unsigned int core = (unsigned int)core_s;

#ifndef BYPASS_M4U_DBG
#ifdef CONFIG_MTK_IOMMU_V2
	struct ion_handle *vpu_handle = NULL;
	struct ion_mm_data vpu_data;
	unsigned long fix_mva = 0;
#endif
	uint32_t mva_reset_vector;
	uint32_t mva_main_program;
#ifdef CONFIG_MTK_M4U
	uint32_t mva_algo_binary_data;
	uint32_t mva_iram_data;
	struct sg_table *sg;
#endif
	uint64_t binpa_reset_vector;
	uint64_t binpa_main_program;
	uint64_t binpa_iram_data;
	const uint64_t size_algos = VPU_SIZE_BINARY_CODE -
				VPU_OFFSET_ALGO_AREA -
		(VPU_SIZE_BINARY_CODE - VPU_OFFSET_MAIN_PROGRAM_IMEM);

	LOG_DBG("%s, bin_pa(0x%lx)\n", __func__,
			(unsigned long)bin_pa);

	switch (core) {
	case 0:
	default:
		mva_reset_vector = VPU_MVA_RESET_VECTOR;
		mva_main_program = VPU_MVA_MAIN_PROGRAM;
		binpa_reset_vector = bin_pa;
		binpa_main_program = bin_pa + VPU_OFFSET_MAIN_PROGRAM;
		binpa_iram_data = bin_pa + VPU_OFFSET_MAIN_PROGRAM_IMEM;
		break;
	case 1:
		mva_reset_vector = VPU2_MVA_RESET_VECTOR;
		mva_main_program = VPU2_MVA_MAIN_PROGRAM;
		binpa_reset_vector = bin_pa +
						VPU_DDR_SHIFT_RESET_VECTOR;

		binpa_main_program = binpa_reset_vector +
						VPU_OFFSET_MAIN_PROGRAM;

		binpa_iram_data = bin_pa +
					VPU_OFFSET_MAIN_PROGRAM_IMEM +
					VPU_DDR_SHIFT_IRAM_DATA;
		break;
	}
	LOG_DBG("%s(core:%d), pa resvec/mainpro(0x%lx/0x%lx)\n",
		__func__, core,
		(unsigned long)binpa_reset_vector,
		(unsigned long)binpa_main_program);
#ifdef CONFIG_MTK_IOMMU_V2
	/*map reset vector*/
	vpu_handle = ion_alloc(ion_client, VPU_SIZE_RESET_VECTOR,
			       binpa_reset_vector,
			       ION_HEAP_MULTIMEDIA_PA2MVA_MASK, 0);
	if (IS_ERR(vpu_handle)) {
		LOG_ERR("reset-vpu_ion_alloc error %p\n", vpu_handle);
		return -1;
	}

	memset((void *)&vpu_data, 0, sizeof(struct ion_mm_data));
	fix_mva = ((unsigned long)VPU_PORT_OF_IOMMU << 24) |
		  ION_FLAG_GET_FIXED_PHYS;
	vpu_data.get_phys_param.module_id = VPU_PORT_OF_IOMMU;
	vpu_data.get_phys_param.kernel_handle = vpu_handle;
	vpu_data.get_phys_param.reserve_iova_start = mva_reset_vector;
	vpu_data.get_phys_param.reserve_iova_end = IOMMU_VA_END;
	vpu_data.get_phys_param.phy_addr = fix_mva;
	vpu_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
	vpu_data.mm_cmd = ION_MM_GET_IOVA_EXT;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
				 (unsigned long)&vpu_data) < 0) {
		LOG_ERR("main-config buffer failed.%p -%p\n",
			ion_client, vpu_handle);
		ion_free(ion_client, vpu_handle);
		return -1;
	}

	/*map main vector*/
	vpu_handle = ion_alloc(ion_client, VPU_SIZE_MAIN_PROGRAM,
			       binpa_main_program,
			       ION_HEAP_MULTIMEDIA_PA2MVA_MASK, 0);
	if (IS_ERR(vpu_handle)) {
		LOG_ERR("main-vpu_ion_alloc error %p\n", vpu_handle);
		return -1;
	}

	vpu_data.get_phys_param.kernel_handle = vpu_handle;
	vpu_data.get_phys_param.phy_addr = fix_mva;
	vpu_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
	vpu_data.get_phys_param.reserve_iova_start = mva_main_program;
	vpu_data.get_phys_param.reserve_iova_end = IOMMU_VA_END;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
				 (unsigned long)&vpu_data) < 0) {
		LOG_ERR("main-config buffer failed.%p -%p\n",
			ion_client, vpu_handle);
		ion_free(ion_client, vpu_handle);
		return -1;
	}

	if (core == 0) {
		vpu_handle = ion_alloc(ion_client, size_algos,
				       bin_pa + VPU_OFFSET_ALGO_AREA,
				       ION_HEAP_MULTIMEDIA_PA2MVA_MASK, 0);
		if (IS_ERR(vpu_handle)) {
			LOG_ERR("main-vpu_ion_alloc error %p\n", vpu_handle);
			return -1;
		}

		vpu_data.get_phys_param.kernel_handle = vpu_handle;
		vpu_data.get_phys_param.phy_addr = 0;
		vpu_data.get_phys_param.len = 0;
		vpu_data.mm_cmd = ION_MM_GET_IOVA;
		if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
				 (unsigned long)&vpu_data) < 0) {
			LOG_ERR("algo-config buffer failed.%p -%p\n",
				ion_client, vpu_handle);
			ion_free(ion_client, vpu_handle);
			return -1;
		}

		vpu_service_cores[core].algo_data_mva =
					vpu_data.get_phys_param.phy_addr;
	}

	vpu_handle = ion_alloc(ion_client, VPU_SIZE_MAIN_PROGRAM_IMEM,
			       binpa_iram_data,
			       ION_HEAP_MULTIMEDIA_PA2MVA_MASK, 0);
	if (IS_ERR(vpu_handle)) {
		LOG_ERR("main-vpu_ion_alloc error %p\n", vpu_handle);
		return -1;
	}

	vpu_data.get_phys_param.kernel_handle = vpu_handle;
	vpu_data.get_phys_param.phy_addr = 0;
	vpu_data.get_phys_param.len = 0;
	vpu_data.mm_cmd = ION_MM_GET_IOVA;
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
	    (unsigned long)&vpu_data) < 0) {
		LOG_ERR("iram-config buffer failed.%p -%p\n",
			ion_client, vpu_handle);
		ion_free(ion_client, vpu_handle);
		return -1;
	}

	vpu_service_cores[core].iram_data_mva =
				vpu_data.get_phys_param.phy_addr;
#else
	/* 1. map reset vector */
	sg = &(vpu_service_cores[core].sg_reset_vector);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to allocate sg table[reset]!\n",
				core);
		goto out;
	}
	LOG_DBG("vpu...sg_alloc_table ok\n");

	sg_dma_address(sg->sgl) = binpa_reset_vector;

	LOG_DBG("vpu...sg_dma_address ok, bin_pa(0x%x)\n",
			(unsigned int)binpa_reset_vector);

	sg_dma_len(sg->sgl) = VPU_SIZE_RESET_VECTOR;

	LOG_DBG("vpu...sg_dma_len ok, VPU_SIZE_RESET_VECTOR(0x%x)\n",
			VPU_SIZE_RESET_VECTOR);

	ret = m4u_alloc_mva(m4u_client, VPU_PORT_OF_IOMMU,
			0, sg,
			VPU_SIZE_RESET_VECTOR,
			M4U_PROT_READ | M4U_PROT_WRITE,
			M4U_FLAGS_START_FROM, &mva_reset_vector);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to allocate mva of reset vecter!\n",
				core);
		goto out;
	}
	LOG_DBG("vpu...m4u_alloc_mva ok\n");

	/* 2. map main program */
	sg = &(vpu_service_cores[core].sg_main_program);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to allocate sg table[main]!\n",
				core);
		goto out;
	}
	LOG_DBG("vpu...sg_alloc_table main_program ok\n");

	sg_dma_address(sg->sgl) = binpa_main_program;
	sg_dma_len(sg->sgl) = VPU_SIZE_MAIN_PROGRAM;
	ret = m4u_alloc_mva(m4u_client, VPU_PORT_OF_IOMMU,
			0, sg,
			VPU_SIZE_MAIN_PROGRAM,
			M4U_PROT_READ | M4U_PROT_WRITE,
			M4U_FLAGS_START_FROM, &mva_main_program);
	if (ret) {
		LOG_ERR("fail to allocate mva of main program!\n");

		m4u_dealloc_mva(m4u_client, VPU_PORT_OF_IOMMU,
					mva_main_program);
		goto out;
	}

	LOG_DBG("vpu...m4u_alloc_mva main_program ok, (0x%x/0x%x)\n",
			(unsigned int)(binpa_main_program),
			(unsigned int)VPU_SIZE_MAIN_PROGRAM);

	/* 3. map all algo binary data(src addr for dps to copy) */
	/* no need reserved mva, use SG_READY*/
	if (core == 0) {
		sg = &(vpu_service_cores[core].sg_algo_binary_data);
		ret = sg_alloc_table(sg, 1, GFP_KERNEL);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to allocate sg table[reset]!\n",
					core);
			goto out;
		}

		LOG_DBG("vpu...sg_alloc_table algo_data ok, %s=0x%x\n",
				"mva_algo_binary_data", mva_algo_binary_data);

		sg_dma_address(sg->sgl) = bin_pa + VPU_OFFSET_ALGO_AREA;
		sg_dma_len(sg->sgl) = size_algos;

		ret = m4u_alloc_mva(m4u_client, VPU_PORT_OF_IOMMU,
				0, sg,
				size_algos,
				M4U_PROT_READ | M4U_PROT_WRITE,
				M4U_FLAGS_SG_READY, &mva_algo_binary_data);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to allocate %s!\n",
					core,
					"mva of reset vecter");
			goto out;
		}

		vpu_service_cores[core].algo_data_mva = mva_algo_binary_data;

		LOG_DBG("a vpu va_algo_data pa: 0x%x\n",
				(unsigned int)(bin_pa + VPU_OFFSET_ALGO_AREA));

		LOG_DBG("a vpu va_algo_data: 0x%x/0x%x, size: 0x%x\n",
			mva_algo_binary_data,
			(unsigned int)(vpu_service_cores[core].algo_data_mva),
			(unsigned int)size_algos);
	} else {
		vpu_service_cores[core].algo_data_mva =
				vpu_service_cores[0].algo_data_mva;
	}

	/* 4. map main program iram data */
	/* no need reserved mva, use SG_READY*/
	sg = &(vpu_service_cores[core].sg_iram_data);
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to allocate sg table[reset]!\n", core);
		goto out;
	}

	LOG_DBG("vpu...sg_alloc_table iram_data ok, mva_iram_data = 0x%x\n",
			mva_iram_data);

	LOG_DBG("a vpu iram pa: 0x%lx\n", (unsigned long)(binpa_iram_data));

	sg_dma_address(sg->sgl) = binpa_iram_data;
	sg_dma_len(sg->sgl) = VPU_SIZE_MAIN_PROGRAM_IMEM;
	ret = m4u_alloc_mva(m4u_client, VPU_PORT_OF_IOMMU,
			0, sg,
			VPU_SIZE_MAIN_PROGRAM_IMEM,
			M4U_PROT_READ | M4U_PROT_WRITE,
			M4U_FLAGS_SG_READY, &mva_iram_data);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to allocate mva of iram data!\n", core);
		goto out;
	}

	vpu_service_cores[core].iram_data_mva = (uint64_t)(mva_iram_data);
	LOG_DBG("a vpu va_iram_data: 0x%x, iram_data_mva: 0x%lx\n",
			mva_iram_data,
			(unsigned long)(vpu_service_cores[core].iram_data_mva));

out:
#endif
#else
	LOG_DBG("%s bypass\n", __func__);
#endif
	return ret;
}

#endif

int vpu_get_default_algo_num(int core_s, vpu_id_t *algo_num)
{
	int ret = 0;
	unsigned int core = (unsigned int)core_s;

	*algo_num = vpu_service_cores[core].default_algo_num;

	return ret;
}

int vpu_get_power(int core, bool secure)
{
	int ret = 0;

	LOG_DBG("[vpu_%d/%d] gp +\n", core, power_counter[core]);
	mutex_lock(&power_counter_mutex[core]);
	power_counter[core]++;
	ret = vpu_boot_up(core, secure);
	mutex_unlock(&power_counter_mutex[core]);
	LOG_DBG("[vpu_%d/%d] gp + 2\n", core, power_counter[core]);
	if (ret == POWER_ON_MAGIC) {
		mutex_lock(&opp_mutex);
		if (change_freq_first[core]) {
			LOG_DBG("[vpu_%d] change freq first(%d)\n",
					core, change_freq_first[core]);
			/*mutex_unlock(&opp_mutex);*/
			/*mutex_lock(&opp_mutex);*/
			if (force_change_dsp_freq[core]) {
				mutex_unlock(&opp_mutex);
				/* force change freq while running */
				LOG_DBG("vpu_%d force change dsp freq", core);
				vpu_change_opp(core, OPPTYPE_DSPFREQ);
			} else {
				mutex_unlock(&opp_mutex);
			}

			mutex_lock(&opp_mutex);
			//if (force_change_vcore_opp[core]) {
			if (force_change_vvpu_opp[core]) {
				mutex_unlock(&opp_mutex);
				/* vcore change should wait */
				LOG_DBG("vpu_%d force change vvpu opp", core);
				//vpu_change_opp(core, OPPTYPE_VCORE);
				vpu_change_opp(core, OPPTYPE_VVPU);
			} else {
				mutex_unlock(&opp_mutex);
			}
		} else {
			/*mutex_unlock(&opp_mutex);*/
			/*mutex_lock(&opp_mutex);*/
			//if (force_change_vcore_opp[core]) {
			if (force_change_vvpu_opp[core]) {
				mutex_unlock(&opp_mutex);
				/* vcore change should wait */
				LOG_DBG("vpu_%d force change vcore opp", core);
				//vpu_change_opp(core, OPPTYPE_VCORE);
				vpu_change_opp(core, OPPTYPE_VVPU);
			} else {
				mutex_unlock(&opp_mutex);
			}

			mutex_lock(&opp_mutex);
			if (force_change_dsp_freq[core]) {
				mutex_unlock(&opp_mutex);
				/* force change freq while running */
				LOG_DBG("vpu_%d force change dsp freq", core);
				vpu_change_opp(core, OPPTYPE_DSPFREQ);
			} else {
				mutex_unlock(&opp_mutex);
			}
		}
	}
	if (g_vpu_log_level > Log_STATE_MACHINE)
		apu_get_power_info();
	LOG_DBG("[vpu_%d/%d] gp -\n", core, power_counter[core]);
	enable_apu_bw(0);
	enable_apu_bw(1);
	enable_apu_bw(2);
	enable_apu_latency(0);
	enable_apu_latency(1);
	enable_apu_latency(2);
	if (core == 0)
		apu_power_count_enable(true, USER_VPU0);
	else if (core == 1)
		apu_power_count_enable(true, USER_VPU1);
	if (ret == POWER_ON_MAGIC)
		return 0;
	else
		return ret;
}

void vpu_put_power(int core_s, enum VpuPowerOnType type)
{
	unsigned int core = (unsigned int)core_s;
	LOG_DBG("[vpu_%d/%d] pp +\n", core, power_counter[core]);
	mutex_lock(&power_counter_mutex[core]);
	if (--power_counter[core] == 0) {
		switch (type) {
		case VPT_PRE_ON:
			LOG_DBG("[vpu_%d] VPT_PRE_ON\n", core);
			mod_delayed_work(wq,
				&(power_counter_work[core].my_work),
				msecs_to_jiffies(10 * PWR_KEEP_TIME_MS));
			break;
		case VPT_IMT_OFF:
			LOG_INF("[vpu_%d] VPT_IMT_OFF\n", core);
			mod_delayed_work(wq,
				&(power_counter_work[core].my_work),
				msecs_to_jiffies(0));
			break;
		case VPT_ENQUE_ON:
		default:
			LOG_DBG("[vpu_%d] VPT_ENQUE_ON\n", core);
			mod_delayed_work(wq,
				&(power_counter_work[core].my_work),
				msecs_to_jiffies(PWR_KEEP_TIME_MS));
			break;
		}
	}
	mutex_unlock(&power_counter_mutex[core]);
	LOG_DBG("[vpu_%d/%d] pp -\n", core, power_counter[core]);
}

int vpu_set_power(struct vpu_user *user, struct vpu_power *power)
{
	int ret = 0;
	uint8_t vcore_opp_index = 0xFF;
	uint8_t vvpu_opp_index = 0xFF;
	uint8_t dsp_freq_index = 0xFF;
	unsigned int i = 0, core = 0xFFFFFFFF;

	mutex_lock(&set_power_mutex);

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		/*LOG_DBG("debug i(%d), (0x1 << i) (0x%x)", i, (0x1 << i));*/
		if (power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
				power->core, core, MTK_VPU_CORE);
		ret = -1;
		mutex_unlock(&set_power_mutex);
		return ret;
	}

	LOG_INF("[vpu_%d] set power opp:%d, pid=%d, tid=%d\n",
			core, power->opp_step,
			user->open_pid, user->open_tgid);

	if (power->opp_step == 0xFF) {
		vcore_opp_index = 0xFF;
		vvpu_opp_index = 0xFF;
		dsp_freq_index = 0xFF;
	} else {
		if (power->opp_step < VPU_MAX_NUM_OPPS &&
							power->opp_step >= 0) {
			vcore_opp_index = opps.vcore.opp_map[power->opp_step];
			vvpu_opp_index = opps.vvpu.opp_map[power->opp_step];
			dsp_freq_index =
				opps.dspcore[core].opp_map[power->opp_step];
		} else {
			LOG_ERR("wrong opp step (%d)", power->opp_step);
			ret = -1;
			mutex_unlock(&set_power_mutex);
			return ret;
		}
	}

	vpu_opp_check(core, vvpu_opp_index, dsp_freq_index);
	user->power_opp = power->opp_step;

	ret = vpu_get_power(core, false);
	mutex_unlock(&set_power_mutex);
	mutex_lock(&(vpu_service_cores[core].state_mutex));
	vpu_service_cores[core].state = VCT_IDLE;
	mutex_unlock(&(vpu_service_cores[core].state_mutex));

	/* to avoid power leakage, power on/off need be paired */
	vpu_put_power(core, VPT_PRE_ON);
	LOG_INF("[vpu_%d] %s -\n", core, __func__);
	return ret;
}

int vpu_sdsp_get_power(struct vpu_user *user)
{
	int ret = 0;
	uint8_t vcore_opp_index = 0; /*0~15, 0 is max*/
	uint8_t dsp_freq_index = 0;  /*0~15, 0 is max*/
	int core = 0;

	if (sdsp_power_counter == 0) {
		for (core = 0 ; core < MTK_VPU_CORE ; core++) {
			vpu_opp_check(core, vcore_opp_index, dsp_freq_index);

			ret = ret | vpu_get_power(core, true);
			mutex_lock(&(vpu_service_cores[core].state_mutex));
			vpu_service_cores[core].state = VCT_IDLE;
			mutex_unlock(&(vpu_service_cores[core].state_mutex));
		}
	}
	sdsp_power_counter++;
	mod_delayed_work(wq, &sdsp_work,
		msecs_to_jiffies(SDSP_KEEP_TIME_MS));

	LOG_INF("[vpu] %s -\n", __func__);
	return ret;
}

int vpu_sdsp_put_power(struct vpu_user *user)
{
	int ret = 0;
	int core = 0;

	sdsp_power_counter--;

	if (sdsp_power_counter == 0) {
		for (core = 0 ; core < MTK_VPU_CORE ; core++) {
			while (power_counter[core] != 0)
				vpu_put_power(core, VPT_IMT_OFF);

			while (is_power_on[core] == true)
				usleep_range(100, 500);

			LOG_INF("[vpu] power_counter[%d] = %d/%d -\n",
				core, power_counter[core], is_power_on[core]);

		}
	}
	mod_delayed_work(wq,
		&sdsp_work,
		msecs_to_jiffies(0));

	LOG_INF("[vpu] %s, sdsp_power_counter = %d -\n",
		__func__, sdsp_power_counter);
	return ret;
}

static void vpu_power_counter_routine(struct work_struct *work)
{
	int core = 0;
	struct my_struct_t *my_work_core =
			container_of(work, struct my_struct_t, my_work.work);

	core = my_work_core->core;
	LOG_DVFS("vpu_%d counterR (%d)+\n", core, power_counter[core]);

	mutex_lock(&power_counter_mutex[core]);
	if (power_counter[core] == 0)
		vpu_shut_down(core);
	else
		LOG_DBG("vpu_%d no need this time.\n", core);
	mutex_unlock(&power_counter_mutex[core]);

	LOG_DVFS("vpu_%d counterR -", core);
}

bool vpu_is_idle(int core_s)
{
	bool idle = false;
	unsigned int core = (unsigned int)core_s;

	mutex_lock(&(vpu_service_cores[core].state_mutex));

	if (vpu_service_cores[core].state == VCT_SHUTDOWN ||
		vpu_service_cores[core].state == VCT_IDLE)
		idle = true;

	mutex_unlock(&(vpu_service_cores[core].state_mutex));

	LOG_INF("%s vpu_%d, idle = %d, state = %d  !!\r\n", __func__,
		core, idle, vpu_service_cores[core].state);

	return idle;

}

int vpu_quick_suspend(int core_s)
{
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("[vpu_%d] q_suspend +\n", core);
	mutex_lock(&power_counter_mutex[core]);
	LOG_INF("[vpu_%d] q_suspend (%d/%d)\n", core,
		power_counter[core], vpu_service_cores[core].state);

	if (power_counter[core] == 0) {
		mutex_unlock(&power_counter_mutex[core]);

		mutex_lock(&(vpu_service_cores[core].state_mutex));

		switch (vpu_service_cores[core].state) {
		case VCT_SHUTDOWN:
		case VCT_NONE:
			/* vpu has already been shut down, do nothing*/
			mutex_unlock(&(vpu_service_cores[core].state_mutex));
			break;
		case VCT_IDLE:
		case VCT_BOOTUP:
		case VCT_EXECUTING:
		case VCT_VCORE_CHG:
		default:
			mutex_unlock(&(vpu_service_cores[core].state_mutex));
			mod_delayed_work(wq,
				&(power_counter_work[core].my_work),
				msecs_to_jiffies(0));
			break;
		}
	} else {
		mutex_unlock(&power_counter_mutex[core]);
	}

	return 0;
}


static void vpu_opp_keep_routine(struct work_struct *work)
{
	LOG_DVFS("%s flag (%d) +\n", __func__, opp_keep_flag);
	mutex_lock(&opp_mutex);
	opp_keep_flag = false;
	mutex_unlock(&opp_mutex);
	LOG_DVFS("%s flag (%d) -\n", __func__, opp_keep_flag);
}

static void vpu_sdsp_routine(struct work_struct *work)
{

	if (sdsp_power_counter != 0) {
		LOG_INF("%s long time not unlock!!! -\n", __func__);
		LOG_ERR("%s long time not unlock error!!! -\n", __func__);
	} else {
		LOG_INF("%s sdsp_power_counter is correct!!! -\n", __func__);
	}
}

#ifdef CONFIG_MTK_DEVINFO
#define EFUSE_FIELD_CNT		(2)
static const char *efuse_field[EFUSE_FIELD_CNT] = {
	"efuse_data",
	"efuse_segment",
};

static int get_nvmem_cell_efuse(struct device *dev)
{
	struct nvmem_cell *cell[EFUSE_FIELD_CNT];
	uint32_t *buf[EFUSE_FIELD_CNT];
	int i;

	for (i = 0 ; i < EFUSE_FIELD_CNT ; ++i) {
		cell[i] = nvmem_cell_get(dev, efuse_field[i]);
		if (IS_ERR(cell[i])) {
			LOG_ERR("[%s] nvmem_cell_get fail\n", __func__);
			if (PTR_ERR(cell[i]) == -EPROBE_DEFER)
				return PTR_ERR(cell[i]);
			return -1;
		}

		buf[i] = (uint32_t *)nvmem_cell_read(cell[i], NULL);
		nvmem_cell_put(cell[i]);

		if (IS_ERR(buf[i])) {
			LOG_ERR("[%s] nvmem_cell_read fail\n", __func__);
			return PTR_ERR(buf[i]);
		}

		if (i == 0)
			g_efuse_data = *buf[0];
		if (i == 1)
			g_efuse_segment = *buf[1];

		kfree(buf[i]);
	}

	return 0;
}
#endif // CONFIG_MTK_DEVINFO

int vpu_init_hw(int core_s, struct vpu_device *device)
{
	int ret, i, j;
	int param;
	unsigned int core = (unsigned int)core_s;
	struct vpu_shared_memory_param mem_param;

	vpu_dump_exception = 0;

	vpu_service_cores[core].vpu_base = device->vpu_base[core];
	vpu_service_cores[core].bin_base = device->bin_base;

#ifdef MTK_VPU_EMULATOR
	vpu_request_emulator_irq(device->irq_num[core], vpu_isr_handler);
#else
#ifdef CONFIG_MTK_M4U
	if (!m4u_client)
		m4u_client = m4u_create_client();
#endif
	if (!ion_client)
		ion_client = ion_client_create(g_ion_device, "vpu");

	ret = vpu_map_mva_of_bin(core, device->bin_pa);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to map binary data!\n", core);
		goto out;
	}

	vpu_total_algo_num(core);

	ret = request_irq(device->irq_num[core],
		(irq_handler_t)VPU_ISR_CB_TBL[core].isr_fp,
		device->irq_trig_level,
		(const char *)VPU_ISR_CB_TBL[core].device_name, NULL);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to request vpu irq!\n", core);
		goto out;
	}
#endif

	sdsp_power_counter = 0;

	if (core == 0) {
		init_waitqueue_head(&cmd_wait);
		mutex_init(&lock_mutex);
		init_waitqueue_head(&lock_wait);
		mutex_init(&opp_mutex);
		mutex_init(&power_lock_mutex);
		mutex_init(&set_power_mutex);
		init_waitqueue_head(&waitq_change_vcore);
		init_waitqueue_head(&waitq_do_core_executing);
		is_locked = false;
		max_vcore_opp = 0;
		max_vvpu_opp = 0;
		max_dsp_freq = 0;
		opp_keep_flag = false;
		vpu_dev = device;
		is_power_debug_lock = false;

#ifdef MTK_VPU_FPGA_PORTING
		vpu_dev->vpu_hw_support[0] = true;
		vpu_dev->vpu_hw_support[1] = true;
#else
#ifdef CONFIG_MTK_DEVINFO
		get_nvmem_cell_efuse(device->dev[0]);
		efuse_data = (get_devinfo_with_index(3) & 0xC00) >> 10;
		LOG_INF("efuse_data: efuse_data(0x%x)\n", efuse_data);
		switch (efuse_data) {
		case 0x3: /*b11*/
			vpu_dev->vpu_hw_support[0] = false;
			vpu_dev->vpu_hw_support[1] = false;
			break;
		case 0x0:
		default:
			vpu_dev->vpu_hw_support[0] = true;
			vpu_dev->vpu_hw_support[1] = true;
			break;
		case 0x2: /*b10*/
			vpu_dev->vpu_hw_support[0] = true;
			vpu_dev->vpu_hw_support[1] = false;
			break;
		}
#endif
		get_segment_from_efuse();
#endif

		for (i = 0 ; i < MTK_VPU_CORE ; i++) {
			mutex_init(&(power_mutex[i]));
			mutex_init(&(power_counter_mutex[i]));
			power_counter[i] = 0;
			power_counter_work[i].core = i;
			is_power_on[i] = false;
			force_change_vcore_opp[i] = false;
			force_change_vvpu_opp[i] = false;
			force_change_dsp_freq[i] = false;
			change_freq_first[i] = false;
			exception_isr_check[i] = false;

			INIT_DELAYED_WORK(&(power_counter_work[i].my_work),
						vpu_power_counter_routine);
#ifdef MET_VPU_LOG
			/* Init ftrace dumpwork information */
			spin_lock_init(&(ftrace_dump_work[i].my_lock));
			INIT_LIST_HEAD(&(ftrace_dump_work[i].list));
			INIT_WORK(&(ftrace_dump_work[i].my_work),
				vpu_dump_ftrace_workqueue);
#endif

			#ifdef CONFIG_PM_SLEEP
			if (i == 0) {
				vpu_wake_lock[i] = wakeup_source_register(NULL,
							"vpu_wakelock_0");
			} else {
				vpu_wake_lock[i] = wakeup_source_register(NULL,
							"vpu_wakelock_1");
			}
			#endif

			if (vpu_dev->vpu_hw_support[i]) {
				param = i;
				vpu_service_cores[i].thread_var = i;
				if (i == 0) {
					vpu_service_cores[i].srvc_task =
					kthread_create(vpu_service_routine,
					    &(vpu_service_cores[i].thread_var),
					    "vpu0");
				} else {
					vpu_service_cores[i].srvc_task =
					kthread_create(vpu_service_routine,
					    &(vpu_service_cores[i].thread_var),
					    "vpu1");
				}
				if (IS_ERR(vpu_service_cores[i].srvc_task)) {
					ret =
				       PTR_ERR(vpu_service_cores[i].srvc_task);
					goto out;
				}
#ifdef MET_VPU_LOG
				/* to save correct pid */
				ftrace_dump_work[i].pid =
				vpu_service_cores[i].srvc_task->pid;
#endif
			wake_up_process(vpu_service_cores[i].srvc_task);
			}

			mutex_init(&(vpu_service_cores[i].cmd_mutex));
			vpu_service_cores[i].is_cmd_done = false;
			mutex_init(&(vpu_service_cores[i].state_mutex));
			vpu_service_cores[i].state = VCT_SHUTDOWN;

			/* working buffer */
			/* no need in reserved region */
			mem_param.require_pa = true;
			mem_param.require_va = true;
			mem_param.size = VPU_SIZE_WORK_BUF;
			mem_param.fixed_addr = 0;

			ret = vpu_alloc_shared_memory(
					&(vpu_service_cores[i].work_buf),
					&mem_param);

			LOG_INF("core(%d):work_buf va (0x%lx),pa(0x%x)\n",
				i,
				(unsigned long)
				    (vpu_service_cores[i].work_buf->va),
				vpu_service_cores[i].work_buf->pa);
			if (ret) {
				LOG_ERR("core(%d):fail to allocate %s!\n",
					i,
					"working buffer");
				goto out;
			}

			/* execution kernel library */
			/* need in reserved region, set end as the end of
			 * reserved address, m4u use start-from
			 */
			mem_param.require_pa = true;
			mem_param.require_va = true;
			mem_param.size = VPU_SIZE_ALGO_KERNEL_LIB;
			switch (i) {
			case 0:
			default:
				mem_param.fixed_addr = VPU_MVA_KERNEL_LIB;
				break;
			case 1:
				mem_param.fixed_addr = VPU2_MVA_KERNEL_LIB;
				break;
			}

			ret = vpu_alloc_shared_memory(
					&(vpu_service_cores[i].exec_kernel_lib),
					&mem_param);

			LOG_INF("core(%d):kernel_lib va (0x%lx),pa(0x%x)\n",
				i,
				(unsigned long)
				    (vpu_service_cores[i].exec_kernel_lib->va),
				vpu_service_cores[i].exec_kernel_lib->pa);
			if (ret) {
				LOG_ERR("core(%d):fail to allocate %s!\n",
						i,
						"kernel_lib buffer");
				goto out;
			}
		}
		/* multi-core shared  */
		/* need in reserved region, set end as the end of reserved
		 * address, m4u use start-from
		 */
		mem_param.require_pa = true;
		mem_param.require_va = true;
		mem_param.size = VPU_SIZE_SHARED_DATA;
		mem_param.fixed_addr = VPU_MVA_SHARED_DATA;
		ret = vpu_alloc_shared_memory(&(core_shared_data), &mem_param);
		LOG_DBG("shared_data va (0x%lx),pa(0x%x)",
				(unsigned long)(core_shared_data->va),
				core_shared_data->pa);
		if (ret) {
			LOG_ERR("fail to allocate working buffer!\n");
			goto out;
		}

		wq = create_workqueue("vpu_wq");
		g_func_mask = 0x0;

		vpu_qos_counter_init();

			/* define the steps and OPP */
#define DEFINE_APU_STEP(step, m, v0, v1, v2, v3, \
						v4, v5, v6, v7, v8, v9, \
						v10, v11, v12, v13, v14, v15) \
				{ \
					opps.step.index = m - 1; \
					opps.step.count = m; \
					opps.step.values[0] = v0; \
					opps.step.values[1] = v1; \
					opps.step.values[2] = v2; \
					opps.step.values[3] = v3; \
					opps.step.values[4] = v4; \
					opps.step.values[5] = v5; \
					opps.step.values[6] = v6; \
					opps.step.values[7] = v7; \
					opps.step.values[8] = v8; \
					opps.step.values[9] = v9; \
					opps.step.values[10] = v10; \
					opps.step.values[11] = v11; \
					opps.step.values[12] = v12; \
					opps.step.values[13] = v13; \
					opps.step.values[14] = v14; \
					opps.step.values[15] = v15; \
				}


#define DEFINE_APU_OPP(i, v0, v1, v2, v3, v4, v5, v6) \
			{ \
				opps.vvpu.opp_map[i]  = v0; \
				opps.vmdla.opp_map[i]  = v1; \
				opps.dsp.opp_map[i]    = v2; \
				opps.dspcore[0].opp_map[i]	 = v3; \
				opps.dspcore[1].opp_map[i]	 = v4; \
				opps.ipu_if.opp_map[i] = v5; \
				opps.mdlacore.opp_map[i]    = v6; \
			}

			DEFINE_APU_STEP(vcore, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(vvpu, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(vmdla, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(dsp, 16, 624000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(dspcore[0], 16, 700000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(dspcore[1], 16, 700000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(mdlacore, 16, 788000,
				700000, 624000, 606000,
				594000, 546000, 525000,
				450000, 416000, 364000,
				312000, 273000, 208000,
				137000, 52000, 26000);
			DEFINE_APU_STEP(ipu_if, 16, 624000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);

			/* default freq */
			DEFINE_APU_OPP(0, 0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_OPP(1, 0, 0, 1, 1, 1, 1, 1);
			DEFINE_APU_OPP(2, 0, 0, 2, 2, 2, 2, 2);
			DEFINE_APU_OPP(3, 0, 1, 3, 3, 3, 3, 3);
			DEFINE_APU_OPP(4, 0, 1, 4, 4, 4, 4, 4);
			DEFINE_APU_OPP(5, 1, 1, 5, 5, 5, 5, 5);
			DEFINE_APU_OPP(6, 1, 1, 6, 6, 6, 6, 6);
			DEFINE_APU_OPP(7, 1, 1, 7, 7, 7, 7, 7);
			DEFINE_APU_OPP(8, 1, 1, 8, 8, 8, 8, 8);
			DEFINE_APU_OPP(9, 2, 2, 9, 9, 9, 9, 9);
			DEFINE_APU_OPP(10, 2, 2, 10, 10, 10, 10, 10);
			DEFINE_APU_OPP(11, 2, 2, 11, 11, 11, 11, 11);
			DEFINE_APU_OPP(12, 2, 2, 12, 12, 12, 12, 12);
			DEFINE_APU_OPP(13, 2, 2, 13, 13, 13, 13, 13);
			DEFINE_APU_OPP(14, 2, 2, 14, 14, 14, 14, 14);
			DEFINE_APU_OPP(15, 2, 2, 15, 15, 15, 15, 15);


			/* default low opp */
			opps.count = 16;
			opps.index = 4; /* user space usage*/
			opps.vvpu.index = 1;
			opps.dsp.index = 9;
			opps.dspcore[0].index = 9;
			opps.dspcore[1].index = 9;
			opps.ipu_if.index = 9;
			opps.mdlacore.index = 9;
#undef DEFINE_APU_OPP
#undef DEFINE_APU_STEP

		ret = vpu_prepare_regulator_and_clock(vpu_dev->dev[core]);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to prepare regulator or clk!\n",
					core);
			goto out;
		}

		/* pmqos  */
		#ifdef ENABLE_PMQOS
		for (i = 0 ; i < MTK_VPU_CORE ; i++) {
			/*
			 * mtk_pm_qos_add_request(&vpu_qos_bw_request[i],
			 *			MTK_PM_QOS_MEMORY_EXT_BANDWIDTH,
			 *			PM_QOS_DEFAULT_VALUE);
			 */

			mtk_pm_qos_add_request(&vpu_qos_vcore_request[i],
					MTK_PM_QOS_VCORE_OPP,
					MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);

		    mtk_pm_qos_add_request(&vpu_qos_vvpu_request[i],
					    MTK_PM_QOS_VVPU_OPP,
					    MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE);
		    mtk_pm_qos_add_request(&vpu_qos_vmdla_request[i],
					    MTK_PM_QOS_VMDLA_OPP,
					    MTK_PM_QOS_VMDLA_OPP_DEFAULT_VALUE);
		mtk_pm_qos_update_request(&vpu_qos_vvpu_request[i],
							VVPU_OPP_2);
		mtk_pm_qos_update_request(&vpu_qos_vmdla_request[i],
							VMDLA_OPP_2);
		}
		LOG_INF("[vpu]init vvpu, vmdla to opp2\n");
		ret = vmdla_regulator_set_mode(true);
		udelay(100);
		LOG_INF("vvpu set sleep mode ret=%d\n", ret);
		ret = vmdla_regulator_set_mode(false);
		LOG_INF("vvpu set sleep mode ret=%d\n", ret);
		udelay(100);
		#endif

	}
	/*init vpu lock power struct*/
for (i = 0 ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
	for (j = 0 ; j < MTK_VPU_CORE ; j++) {
		lock_power[i][j].core = 0;
		lock_power[i][j].max_boost_value = 100;
		lock_power[i][j].min_boost_value = 0;
		lock_power[i][j].lock = false;
		lock_power[i][j].priority = NORMAL;
		min_opp[j] = VPU_MAX_NUM_OPPS-1;
		max_opp[j] = 0;
		min_boost[j] = 0;
		max_boost[j] = 100;
		}
	}
	vpu_init_done = 1;
	return 0;

out:

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_service_cores[i].srvc_task != NULL) {
			kthread_stop(vpu_service_cores[i].srvc_task);
			vpu_service_cores[i].srvc_task = NULL;
		}

		if (vpu_service_cores[i].work_buf)
			vpu_free_shared_memory(vpu_service_cores[i].work_buf);
	}


	return ret;
}

int vpu_uninit_hw(void)
{
	int i;

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		cancel_delayed_work(&(power_counter_work[i].my_work));

		if (vpu_service_cores[i].srvc_task != NULL) {
			kthread_stop(vpu_service_cores[i].srvc_task);
			vpu_service_cores[i].srvc_task = NULL;
		}

		if (vpu_service_cores[i].work_buf) {
			vpu_free_shared_memory(vpu_service_cores[i].work_buf);
			vpu_service_cores[i].work_buf = NULL;
		}
	}
	cancel_delayed_work(&opp_keep_work);
	/* pmqos  */
	#ifdef ENABLE_PMQOS
	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		//mtk_pm_qos_remove_request(&vpu_qos_bw_request[i]);
		mtk_pm_qos_remove_request(&vpu_qos_vcore_request[i]);
		mtk_pm_qos_remove_request(&vpu_qos_vvpu_request[i]);
		mtk_pm_qos_remove_request(&vpu_qos_vmdla_request[i]);
	}
	#endif

	vpu_unprepare_regulator_and_clock();
#ifdef CONFIG_MTK_m4U
	if (m4u_client) {
		m4u_destroy_client(m4u_client);
		m4u_client = NULL;
	}
#endif
	if (ion_client) {
		ion_client_destroy(ion_client);
		ion_client = NULL;
	}

	if (wq) {
		flush_workqueue(wq);
		destroy_workqueue(wq);
		wq = NULL;
	}
	vpu_init_done = 0;

	vpu_qos_counter_destroy();
	return 0;
}


static int vpu_check_precond(int core)
{
	uint32_t status;
	size_t i;

	/* wait 1 seconds, if not ready or busy */
	for (i = 0; i < 50; i++) {
		status = vpu_read_field(core, FLD_XTENSA_INFO00);
		switch (status) {
		case VPU_STATE_READY:
		case VPU_STATE_IDLE:
		case VPU_STATE_ERROR:
			return 0;
		case VPU_STATE_NOT_READY:
		case VPU_STATE_BUSY:
			msleep(20);
			break;
		case VPU_STATE_TERMINATED:
			return -EBADFD;
		}
	}
	LOG_ERR("core(%d) still busy(%d) after wait 1 second.\n", core, status);
	return -EBUSY;
}

static int vpu_check_postcond(int core)
{
	uint32_t status = vpu_read_field(core, FLD_XTENSA_INFO00);

	LOG_DBG("%s (0x%x)\n", __func__, status);

	switch (status) {
	case VPU_STATE_READY:
	case VPU_STATE_IDLE:
		return 0;
	case VPU_STATE_NOT_READY:
	case VPU_STATE_ERROR:
		return -EIO;
	case VPU_STATE_BUSY:
		return -EBUSY;
	case VPU_STATE_TERMINATED:
		return -EBADFD;
	default:
		return -EINVAL;
	}
}

int vpu_hw_enable_jtag(bool enabled)
{
	int ret = 0;
	int TEMP_CORE = 0;

	vpu_get_power(TEMP_CORE, false);

	vpu_write_field(TEMP_CORE, FLD_SPNIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_SPIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_NIDEN, enabled);
	vpu_write_field(TEMP_CORE, FLD_DBG_EN, enabled);

	vpu_put_power(TEMP_CORE, VPT_ENQUE_ON);
	return ret;
}

int vpu_hw_boot_sequence(int core_s)
{
	int ret;
	uint64_t ptr_ctrl;
	uint64_t ptr_reset;
	uint64_t ptr_axi_0;
	uint64_t ptr_axi_1;
	unsigned int reg_value = 0;
	bool is_hw_fail = true;
	unsigned int core = (unsigned int)core_s;

	vpu_trace_begin("%s", __func__);
	LOG_INF("[vpu_%d] boot-seq core(%d)\n", core, core);

	LOG_DBG("CTRL(0x%x)\n",
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x110));
	LOG_DBG("XTENSA_INT(0x%x)\n",
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x114));
	LOG_DBG("CTL_XTENSA_INT(0x%x)\n",
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x118));
	LOG_DBG("CTL_XTENSA_INT_CLR(0x%x)\n",
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x11C));

	lock_command(core, 0x0);
	ptr_ctrl = vpu_service_cores[core].vpu_base +
				g_vpu_reg_descs[REG_CTRL].offset;

	ptr_reset = vpu_service_cores[core].vpu_base +
				g_vpu_reg_descs[REG_SW_RST].offset;

	ptr_axi_0 = vpu_service_cores[core].vpu_base +
				g_vpu_reg_descs[REG_AXI_DEFAULT0].offset;

	ptr_axi_1 = vpu_service_cores[core].vpu_base +
				g_vpu_reg_descs[REG_AXI_DEFAULT1].offset;

	/* 1. write register */
	/* set specific address for reset vector in external boot */
	reg_value = vpu_read_field(core, FLD_CORE_XTENSA_ALTRESETVEC);
	LOG_DBG("vpu bf ALTRESETVEC (0x%x), RV(0x%x)\n",
		reg_value, VPU_MVA_RESET_VECTOR);

	switch (core) {
	case 0:
	default:
		vpu_write_field(core, FLD_CORE_XTENSA_ALTRESETVEC,
					VPU_MVA_RESET_VECTOR);
		break;
	case 1:
		vpu_write_field(core, FLD_CORE_XTENSA_ALTRESETVEC,
					VPU2_MVA_RESET_VECTOR);
		break;
	}

	reg_value = vpu_read_field(core, FLD_CORE_XTENSA_ALTRESETVEC);
	LOG_DBG("vpu af ALTRESETVEC (0x%x), RV(0x%x)\n",
		reg_value, VPU_MVA_RESET_VECTOR);

	VPU_SET_BIT(ptr_ctrl, 31); /* csr_p_debug_enable */
	VPU_SET_BIT(ptr_ctrl, 26); /* debug interface cock gated enable */
	VPU_SET_BIT(ptr_ctrl, 19); /* force to boot based on X_ALTRESETVEC */
	VPU_SET_BIT(ptr_ctrl, 23); /* RUN_STALL pull up */
	VPU_SET_BIT(ptr_ctrl, 17); /* pif gated enable */

	VPU_CLR_BIT(ptr_ctrl, 29);
	VPU_CLR_BIT(ptr_ctrl, 28);
	VPU_CLR_BIT(ptr_ctrl, 27);

	VPU_SET_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 12); /* OCD_HALT_ON_RST pull down */
	vpu_write_field(core, FLD_PRID, core); /* set PRID */
	VPU_SET_BIT(ptr_reset, 4); /* B_RST pull up */
	VPU_SET_BIT(ptr_reset, 8); /* D_RST pull up */
	ndelay(27); /* wait for 27ns */

	VPU_CLR_BIT(ptr_reset, 4); /* B_RST pull down */
	VPU_CLR_BIT(ptr_reset, 8); /* D_RST pull down */
	ndelay(27); /* wait for 27ns */

	/* pif gated disable, to prevent unknown propagate to BUS */
	VPU_CLR_BIT(ptr_ctrl, 17);
#ifndef BYPASS_M4U_DBG
	/*Setting for mt6779*/
	VPU_SET_BIT(ptr_axi_0, 21);
	VPU_SET_BIT(ptr_axi_0, 26);
	VPU_SET_BIT(ptr_axi_1, 3);
	VPU_SET_BIT(ptr_axi_1, 8);
#endif
	/* default set pre-ultra instead of ultra */
	VPU_SET_BIT(ptr_axi_0, 28);

	if (g_vpu_log_level > VpuLogThre_PERFORMANCE) {
		LOG_INF("[vpu_%d] REG_AXI_DEFAULT0(0x%x)\n", core,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x13C));
	}

	LOG_DBG("[vpu_%d] REG_AXI_DEFAULT1(0x%x)\n", core,
			vpu_read_reg32(vpu_service_cores[core].vpu_base,
						CTRL_BASE_OFFSET + 0x140));

	/* 2. trigger to run */
	LOG_DBG("vpu dsp:running (%d/0x%x)\n", core,
				vpu_read_field(core, FLD_SRAM_CONFIGURE));
	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down */
	VPU_CLR_BIT(ptr_ctrl, 23);

	/* 3. wait until done */
	ret = wait_command(core);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	ret |= vpu_check_postcond(core);  /* handle for err/exception isr */

	/* RUN_STALL pull up to avoid fake cmd */
	VPU_SET_BIT(ptr_ctrl, 23);

	vpu_trace_end();
	if (ret) {
		vpu_err_hnd(is_hw_fail, core,
			NULL, "VPU Timeout", "vpu%d: Boot-Up Timeout", core);
		goto out;
	}

	/* 4. check the result of boot sequence */
	ret = vpu_check_postcond(core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to boot vpu!\n", core);
		goto out;
	}

out:
	unlock_command(core);
	vpu_trace_end();
	vpu_write_field(core, FLD_APMCU_INT, 1);

	LOG_INF("[vpu_%d] hw_boot_sequence with clr INT-\n", core);
	return ret;
}

#ifdef MET_VPU_LOG
static int vpu_hw_set_log_option(int core)
{
	int ret;

	/* update log enable and mpu enable */
	lock_command(core, VPU_CMD_SET_FTRACE_LOG);
	/* set vpu internal log enable,disable */
	if (g_func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		vpu_write_field(core, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(core, FLD_XTENSA_INFO05, 1);
	} else {
		vpu_write_field(core, FLD_XTENSA_INFO01,
			VPU_CMD_SET_FTRACE_LOG);
		vpu_write_field(core, FLD_XTENSA_INFO05, 0);
	}
	/* set vpu internal log level */
	vpu_write_field(core, FLD_XTENSA_INFO06, g_vpu_internal_log_level);
	/* clear info18 */
	vpu_write_field(core, FLD_XTENSA_INFO18, 0);
	vpu_write_field(core, FLD_RUN_STALL, 0);
	/* RUN_STALL pull down */
	vpu_write_field(core, FLD_CTL_INT, 1);
	ret = wait_command(core);
	vpu_write_field(core, FLD_RUN_STALL, 1);
	/* RUN_STALL pull up to avoid fake cmd */

	/* 4. check the result */
	ret = vpu_check_postcond(core);
	/*CHECK_RET("[vpu_%d]fail to set debug!\n", core);*/
	unlock_command(core);
	return ret;
}
#endif

int vpu_hw_set_debug(int core_s)
{
	int ret;
	struct timespec now;
	unsigned int device_version = 0x0;
	bool is_hw_fail = true;
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("%s (%d)+\n", __func__, core);
	vpu_trace_begin("%s", __func__);

	lock_command(core, VPU_CMD_SET_DEBUG);

	/* 1. set debug */
	getnstimeofday(&now);
	vpu_write_field(core, FLD_XTENSA_INFO01, VPU_CMD_SET_DEBUG);
	vpu_write_field(core, FLD_XTENSA_INFO19,
					vpu_service_cores[core].iram_data_mva);

	vpu_write_field(core, FLD_XTENSA_INFO21,
					vpu_service_cores[core].work_buf->pa +
								VPU_OFFSET_LOG);

	vpu_write_field(core, FLD_XTENSA_INFO22, VPU_SIZE_LOG_BUF);
	vpu_write_field(core, FLD_XTENSA_INFO23,
					now.tv_sec * 1000000 +
						now.tv_nsec / 1000);

	vpu_write_field(core, FLD_XTENSA_INFO29, HOST_VERSION);

	LOG_INF("[vpu_%d] %s=(0x%lx), INFO01(0x%x), 23(%d)\n",
		core,
		"work_buf->pa + VPU_OFFSET_LOG",
		(unsigned long)(vpu_service_cores[core].work_buf->pa +
							VPU_OFFSET_LOG),
		vpu_read_field(core, FLD_XTENSA_INFO01),
		vpu_read_field(core, FLD_XTENSA_INFO23));

	LOG_DBG("vpu_set ok, running\n");

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");

	/* RUN_STALL pull down*/
	vpu_write_field(core, FLD_RUN_STALL, 0);
	vpu_write_field(core, FLD_CTL_INT, 1);
	LOG_DBG("debug timestamp: %.2lu:%.2lu:%.2lu:%.6lu\n",
			(now.tv_sec / 3600) % (24),
			(now.tv_sec / 60) % (60),
			now.tv_sec % 60,
			now.tv_nsec / 1000);

	/* 3. wait until done */
	ret = wait_command(core);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	ret |= vpu_check_postcond(core);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(core, FLD_RUN_STALL, 1);
	vpu_trace_end();

	/*3-additional. check vpu device/host version is matched or not*/
	device_version = vpu_read_field(core, FLD_XTENSA_INFO20);
	if ((int)device_version < (int)HOST_VERSION) {
		LOG_ERR("[vpu_%d] %s device(0x%x) v.s host(0x%x)\n",
			core,
			"wrong vpu version",
			vpu_read_field(core, FLD_XTENSA_INFO20),
			vpu_read_field(core, FLD_XTENSA_INFO29));
		ret = -EIO;
	}

	if (ret) {
		vpu_err_hnd(is_hw_fail, core,
			NULL, "VPU Timeout", "vpu%d: Set Debug Timeout", core);
		goto out;
	}

	/* 4. check the result */
	ret = vpu_check_postcond(core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to set debug!\n", core);
		goto out;
	}

out:
	unlock_command(core);
#ifdef MET_VPU_LOG
	/* to set vpu ftrace log */
	ret = vpu_hw_set_log_option(core);
#endif
	vpu_trace_end();
	LOG_INF("[vpu_%d] hw_set_debug - version check(0x%x/0x%x)\n",
			core,
			vpu_read_field(core, FLD_XTENSA_INFO20),
			vpu_read_field(core, FLD_XTENSA_INFO29));
	return ret;
}

int vpu_get_name_of_algo(int core_s, int id, char **name)
{
	int i;
	unsigned int tmp = (unsigned int)id;
	struct vpu_image_header *header;
	unsigned int core = (unsigned int)core_s;

	header = (struct vpu_image_header *)
			((uintptr_t)vpu_service_cores[core].bin_base +
				(VPU_OFFSET_IMAGE_HEADERS));

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		if (tmp > header[i].algo_info_count) {
			tmp -= header[i].algo_info_count;
			continue;
		}

		*name = header[i].algo_infos[tmp - 1].name;
		return 0;
	}

	*name = NULL;
	LOG_ERR("algo is not existed, id=%d\n", id);
	return -ENOENT;
}

int vpu_total_algo_num(int core_s)
{
	int i;
	int total = 0;
	struct vpu_image_header *header;
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("[vpu] %s +\n", __func__);

	header = (struct vpu_image_header *)
		((uintptr_t)vpu_service_cores[core].bin_base
		+ (VPU_OFFSET_IMAGE_HEADERS));
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++)
		total += header[i].algo_info_count;

	vpu_service_cores[core].default_algo_num = total;

	return total;
};

int vpu_get_entry_of_algo(int core_s, char *name, int *id,
	unsigned int *mva, int *length)
{
	int i, j;
	int s = 1;
	unsigned int coreMagicNum;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header;
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("[vpu] %s +\n", __func__);
	/* coreMagicNum = ( 0x60 | (0x01 << core) ); */
	/* ignore vpu version */
	coreMagicNum = (0x01 << core);

	header = (struct vpu_image_header *)
		((uintptr_t)vpu_service_cores[core].bin_base +
						(VPU_OFFSET_IMAGE_HEADERS));
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			algo_info = &header[i].algo_infos[j];
			LOG_INF("%s: %s/%s, %s:0x%x, %s:%d, %s:0x%x, 0x%x\n",
					"algo name", name, algo_info->name,
					"core info", (algo_info->vpu_core),
					"input core", core,
					"magicNum", coreMagicNum,
					algo_info->vpu_core & coreMagicNum);
			/* CHRISTODO */
			if ((strcmp(name, algo_info->name) == 0) &&
				(algo_info->vpu_core & coreMagicNum)) {
				LOG_INF("[%d] algo_info->offset(0x%x)/0x%x",
					core,
					algo_info->offset,
				    (unsigned int)
				      (vpu_service_cores[core].algo_data_mva));

				*mva = algo_info->offset -
					 VPU_OFFSET_ALGO_AREA +
					 vpu_service_cores[core].algo_data_mva;

				LOG_INF("[%d] *mva(0x%x/0x%lx), s(%d)",
						core, *mva,
						(unsigned long)(*mva), s);

				*length = algo_info->length;
				*id = s;
				return 0;
			}
			s++;
		}
	}

	*id = 0;
	LOG_ERR("algo is not existed, name=%s\n", name);
	return -ENOENT;
};


int vpu_ext_be_busy(void)
{
	int ret;
	/* CHRISTODO */
	int TEMP_CORE = 0;

	lock_command(TEMP_CORE, VPU_CMD_EXT_BUSY);

	/* 1. write register */
	vpu_write_field(TEMP_CORE, FLD_XTENSA_INFO01, VPU_CMD_EXT_BUSY);

	/* 2. trigger interrupt */
	vpu_write_field(TEMP_CORE, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(TEMP_CORE);

	unlock_command(TEMP_CORE);
	return ret;
}

int vpu_debug_func_core_state(int core_s, enum VpuCoreState state)
{
	unsigned int core = (unsigned int)core_s;
	mutex_lock(&(vpu_service_cores[core].state_mutex));
	vpu_service_cores[core].state = state;
	mutex_unlock(&(vpu_service_cores[core].state_mutex));
	return 0;
}

enum MTK_APUSYS_KERNEL_OP {
	MTK_VPU_SMC_INIT = 0,
	MTK_APUSYS_KERNEL_OP_NUM
};

int vpu_boot_up(int core_s, bool secure)
{
	int ret = 0;
	unsigned int core = (unsigned int)core_s;
	struct arm_smccc_res res;

	/*secure flag is for sdsp force shut down*/

	LOG_DBG("[vpu_%d] boot_up +\n", core);
	mutex_lock(&power_mutex[core]);
	LOG_DBG("[vpu_%d] is_power_on(%d)\n", core, is_power_on[core]);
	if (is_power_on[core]) {
		if (secure) {
			if (power_counter[core] != 1)
				LOG_ERR("vpu_%d power counter %d > 1 .\n",
				core, power_counter[core]);
			LOG_WRN("force shut down for sdsp..\n");
			mutex_unlock(&power_mutex[core]);
			vpu_shut_down(core);
			mutex_lock(&power_mutex[core]);
		} else {
			mutex_unlock(&power_mutex[core]);
			mutex_lock(&(vpu_service_cores[core].state_mutex));
			vpu_service_cores[core].state = VCT_BOOTUP;
			mutex_unlock(&(vpu_service_cores[core].state_mutex));
			wake_up_interruptible(&waitq_change_vcore);
			return POWER_ON_MAGIC;
		}
	}
	LOG_DBG("[vpu_%d] boot_up flag2\n", core);

	vpu_trace_begin("%s", __func__);
	mutex_lock(&(vpu_service_cores[core].state_mutex));
	vpu_service_cores[core].state = VCT_BOOTUP;
	mutex_unlock(&(vpu_service_cores[core].state_mutex));
	wake_up_interruptible(&waitq_change_vcore);

	ret = vpu_enable_regulator_and_clock(core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to enable regulator or clock\n", core);
		goto out;
	}

	if (!secure) {
		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
			MTK_VPU_SMC_INIT,
			0, 0, 0, 0, 0, 0, &res);

		ret = vpu_hw_boot_sequence(core);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to do boot sequence\n", core);
			goto out;
		}

		LOG_DBG("[vpu_%d] vpu_hw_boot_sequence done\n", core);

		ret = vpu_hw_set_debug(core);
		if (ret) {
			LOG_ERR("[vpu_%d]fail to set debug\n", core);
			goto out;
		}
	}

	LOG_DBG("[vpu_%d] vpu_hw_set_debug done\n", core);

#ifdef MET_POLLING_MODE
	if (g_func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		ret = vpu_profile_state_set(core, 1);
		if (ret) {
			LOG_ERR("[vpu_%d] fail to profile_state_set 1\n", core);
			goto out;
		}
	}
#endif

out:
	vpu_trace_end();
	mutex_unlock(&power_mutex[core]);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = opps.dspcore[core].index;

		pob_xpufreq_update(POB_XPUFREQ_VPU, &pxi);
	}
#endif

	vpu_qos_counter_start(core);

	return ret;
}

int vpu_shut_down(int core_s)
{
	int ret = 0;
	unsigned int core = (unsigned int)core_s;

	vpu_qos_counter_end(core);
	if (core == 0)
		apu_power_count_enable(false, USER_VPU0);
	else if (core == 1)
		apu_power_count_enable(false, USER_VPU1);
	apu_shut_down();
	LOG_DBG("[vpu_%d] shutdown +\n", core);
	mutex_lock(&power_mutex[core]);
	if (!is_power_on[core]) {
		mutex_unlock(&power_mutex[core]);
		return 0;
	}

	mutex_lock(&(vpu_service_cores[core].state_mutex));
	switch (vpu_service_cores[core].state) {
	case VCT_SHUTDOWN:
	case VCT_IDLE:
	case VCT_NONE:
#ifdef MTK_VPU_FPGA_PORTING
	case VCT_BOOTUP:
#endif
		vpu_service_cores[core].current_algo = 0;
		vpu_service_cores[core].state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		break;
#ifndef MTK_VPU_FPGA_PORTING
	case VCT_BOOTUP:
#endif
	case VCT_EXECUTING:
	case VCT_VCORE_CHG:
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		goto out;
		/*break;*/
	}

#ifdef MET_POLLING_MODE
	if (g_func_mask & VFM_ROUTINE_PRT_SYSLOG) {
		ret = vpu_profile_state_set(core, 0);
		if (ret) {
			LOG_ERR("[vpu_%d] fail to profile_state_set 0\n", core);
			goto out;
		}
	}
#endif

	vpu_trace_begin("%s", __func__);
	ret = vpu_disable_regulator_and_clock(core);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to disable regulator and clock\n", core);
		goto out;
	}

	wake_up_interruptible(&waitq_change_vcore);
out:
	vpu_trace_end();
	mutex_unlock(&power_mutex[core]);
	LOG_DBG("[vpu_%d] shutdown -\n", core);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = -1;

		pob_xpufreq_update(POB_XPUFREQ_VPU, &pxi);
	}
#endif

	return ret;
}

int vpu_hw_load_algo(int core_s, struct vpu_algo *algo)
{
	int ret;
	bool is_hw_fail = true;
	unsigned int core = (unsigned int)core_s;

	LOG_DBG("[vpu_%d] %s +\n", core, __func__);
	/* no need to reload algo if have same loaded algo*/
	if (vpu_service_cores[core].current_algo == algo->id[core])
		return 0;

	vpu_trace_begin("%s(%d)", __func__, algo->id[core]);
	ret = vpu_get_power(core, false);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to get power!\n", core);
		goto out;
	}

	LOG_DBG("[vpu_%d] %s done\n", core, __func__);

	ret = wait_to_do_vpu_running(core);
	if (ret) {
		LOG_ERR("[vpu_%d] %s, ret=%d\n", core,
			"load_algo fail to wait_to_do_vpu_running!",
			ret);
		goto out;
	}

	mutex_lock(&(vpu_service_cores[core].state_mutex));
	vpu_service_cores[core].state = VCT_EXECUTING;
	mutex_unlock(&(vpu_service_cores[core].state_mutex));

	lock_command(core, VPU_CMD_DO_LOADER);
	LOG_DBG("start to load algo\n");

	ret = vpu_check_precond(core);
	if (ret) {
		LOG_ERR("[vpu_%d]have wrong status before do loader!\n", core);
		goto out;
	}

	LOG_DBG("[vpu_%d] vpu_check_precond done\n", core);

	LOG_DBG("[vpu_%d] algo ptr/length (0x%lx/0x%x)\n", core,
		(unsigned long)algo->bin_ptr, algo->bin_length);
	/* 1. write register */
	vpu_write_field(core, FLD_XTENSA_INFO01, VPU_CMD_DO_LOADER);

	/* binary data's address */
	vpu_write_field(core, FLD_XTENSA_INFO12, algo->bin_ptr);

	/* binary data's length */
	vpu_write_field(core, FLD_XTENSA_INFO13, algo->bin_length);

	vpu_write_field(core, FLD_XTENSA_INFO15,
					opps.dsp.values[opps.dsp.index]);

	vpu_write_field(core, FLD_XTENSA_INFO16,
					opps.ipu_if.values[opps.ipu_if.index]);

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");
	LOG_DBG("[vpu_%d] dsp:running\n", core);

	/* RUN_STALL down */
	vpu_write_field(core, FLD_RUN_STALL, 0);
	vpu_write_field(core, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(core);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	LOG_DBG("[vpu_%d] algo done\n", core);

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(core, FLD_RUN_STALL, 1);
	vpu_trace_end();
	if (ret) {
		vpu_err_hnd(is_hw_fail, core,
			NULL, "VPU Timeout",
			"DO_LOADER (%s) Timeout", algo->name);
		goto out;
	}

	/* 4. update the id of loaded algo */
	vpu_service_cores[core].current_algo = algo->id[core];

out:
	unlock_command(core);
	if (ret) {
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		/*vpu_put_power(core, VPT_IMT_OFF);*/
		/* blocking use same ththread to avoid race between
		 * delayedworkQ and serviceThread
		 */
		mutex_lock(&power_counter_mutex[core]);
		power_counter[core]--;
		mutex_unlock(&power_counter_mutex[core]);
		apu_get_power_info();
		LOG_ERR("[vpu_%d] pr hw error, force shutdown, cnt(%d)\n",
				core, power_counter[core]);
		if (power_counter[core] == 0)
			vpu_shut_down(core);
	} else {
		vpu_put_power(core, VPT_ENQUE_ON);
	}
	vpu_trace_end();
	LOG_DBG("[vpu] %s -\n", __func__);
	return ret;
}

/* do whole processing for enque request, including check algo, load algo,
 * run d2d. minimize timing gap between each step for a single eqneu request
 * and minimize the risk of timing issue
 */
int vpu_hw_processing_request(int core_s, struct vpu_request *request)
{
	int ret;
	struct vpu_algo *algo = NULL;
	bool need_reload = false;
	struct timespec start, end;
	uint64_t latency = 0;
	bool is_hw_fail = true;
	unsigned int core = (unsigned int)core_s;

	mutex_lock(&vpu_dev->sdsp_control_mutex[core]);

	/* step1, enable clocks and boot-up if needed */
	ret = vpu_get_power(core, false);
	if (ret) {
		LOG_ERR("[vpu_%d] fail to get power!\n", core);
		goto out2;
	}
	LOG_DBG("[vpu_%d] vpu_get_power done\n", core);

	/* step2. check algo */
	if (request->algo_id[core] != vpu_service_cores[core].current_algo) {
		struct vpu_user *user = (struct vpu_user *)request->user_id;

		ret = vpu_find_algo_by_id(core,
			request->algo_id[core], &algo, user);
		need_reload = true;
		if (ret) {
			request->status = VPU_REQ_STATUS_INVALID;
			LOG_ERR("[vpu_%d] pr can not find the algo, id=%d\n",
				core, request->algo_id[core]);
			goto out2;
		}
	}

	LOG_INF("%s: vpu%d: algo: %s(%d)\n", __func__,
		core, algo ? algo->name : "", request->algo_id[core]);

	/* step3. do processing, algo loader and d2d*/
	ret = wait_to_do_vpu_running(core);
	if (ret) {
		LOG_ERR("[vpu_%d] %s, ret=%d\n", core,
			"pr load_algo fail to wait_to_do_vpu_running!",
			ret);
		goto out;
	}

	mutex_lock(&(vpu_service_cores[core].state_mutex));
	vpu_service_cores[core].state = VCT_EXECUTING;
	mutex_unlock(&(vpu_service_cores[core].state_mutex));
	/* algo loader if needed */
	if (need_reload) {
		vpu_trace_begin("[vpu_%d] hw_load_algo(%d)",
						core, algo->id[core]);
		lock_command(core, VPU_CMD_DO_LOADER);
		LOG_DBG("start to load algo\n");

		ret = vpu_check_precond(core);
		if (ret) {
			LOG_ERR("[vpu_%d]have wrong status before do loader!\n",
					core);
			goto out;
		}

		LOG_DBG("[vpu_%d] vpu_check_precond done\n", core);

		if (g_vpu_log_level > Log_ALGO_OPP_INFO)
			LOG_INF(
		"[vpu_%d] algo_%d ptr/length (0x%lx/0x%x), bf(%d)\n",
				core, algo->id[core],
				(unsigned long)algo->bin_ptr,
				algo->bin_length,
				vpu_service_cores[core].is_cmd_done);

		/* 1. write register */
		vpu_write_field(core, FLD_XTENSA_INFO01, VPU_CMD_DO_LOADER);

		/* binary data's address */
		vpu_write_field(core, FLD_XTENSA_INFO12, algo->bin_ptr);

		/* binary data's length */
		vpu_write_field(core, FLD_XTENSA_INFO13, algo->bin_length);
		vpu_write_field(core, FLD_XTENSA_INFO15,
					opps.dsp.values[opps.dsp.index]);

		vpu_write_field(core, FLD_XTENSA_INFO16,
					opps.ipu_if.values[opps.ipu_if.index]);

		/* 2. trigger interrupt */
		vpu_trace_begin("[vpu_%d] dsp:load_algo running", core);
		LOG_DBG("[vpu_%d] dsp:load_algo running\n", core);

		/* RUN_STALL down */
		vpu_write_field(core, FLD_RUN_STALL, 0);
		vpu_write_field(core, FLD_CTL_INT, 1);

	if (g_vpu_log_level > Log_STATE_MACHINE) {
		LOG_INF("[0x%lx]:load algo start ",
			(unsigned long)request->request_id);
	}
		/* 3. wait until done */
		ret = wait_command(core);
		if (ret == -ERESTARTSYS)
			is_hw_fail = false;

		/* handle for err/exception isr */
		if (exception_isr_check[core])
			ret |= vpu_check_postcond(core);

		if (g_vpu_log_level > Log_ALGO_OPP_INFO)
			LOG_INF(
		"[vpu_%d] algo_%d %s=0x%lx, %s=%d, %s=%d, %s=%d, %d\n",
			core, algo->id[core],
			"bin_ptr", (unsigned long)algo->bin_ptr,
			"done", vpu_service_cores[core].is_cmd_done,
			"ret", ret,
			"info00", vpu_read_field(core, FLD_XTENSA_INFO00),
			exception_isr_check[core]);
		if (ret) {
			exception_isr_check[core] = false;
			LOG_ERR("[vpu_%d] %s, status(%d/%d), ret(%d), %d\n",
				core,
				"pr load_algo err",
				vpu_read_field(core, FLD_XTENSA_INFO00),
				vpu_service_cores[core].is_cmd_done,
				ret, exception_isr_check[core]);
		} else {
			LOG_DBG("[vpu_%d] temp test1.2\n", core);

			/* RUN_STALL pull up to avoid fake cmd */
			vpu_write_field(core, FLD_RUN_STALL, 1);
		}

		vpu_trace_end();

		if (ret) {
			request->status = VPU_REQ_STATUS_TIMEOUT;
			vpu_err_hnd(is_hw_fail, core,
				request, "VPU Timeout",
				"vpu%d: DO_LOADER Timeout, algo: %s(%d)",
				core, algo->name,
				vpu_service_cores[core].current_algo);
			goto out;
		}

		/* 4. update the id of loaded algo */
		vpu_service_cores[core].current_algo = algo->id[core];
		unlock_command(core);
		vpu_trace_end();
	}

	/* d2d operation */
	vpu_trace_begin("[vpu_%d] hw_processing_request(%d)",
				core, request->algo_id[core]);

	LOG_DBG("start to enque request\n");
	lock_command(core, VPU_CMD_DO_D2D);
	ret = vpu_check_precond(core);
	if (ret) {
		request->status = VPU_REQ_STATUS_BUSY;
		LOG_ERR("error state before enque request!\n");
		goto out;
	}

	memcpy((void *) (uintptr_t)vpu_service_cores[core].work_buf->va,
			request->buffers,
			sizeof(struct vpu_buffer) * request->buffer_count);

	LOG_DBG("[vpu_%d]start d2d, %s(%d/%d), %s(%d), %s(%d/%d,%d), %s(%d)\n",
		core,
		"id/frm", request->algo_id[core], request->frame_magic,
		"bw", request->power_param.bw,
		"algo", vpu_service_cores[core].current_algo,
		request->algo_id[core], need_reload,
		"done", vpu_service_cores[core].is_cmd_done);
	/* 1. write register */
	/* command: d2d */
	vpu_write_field(core, FLD_XTENSA_INFO01, VPU_CMD_DO_D2D);
	/* buffer count */
	vpu_write_field(core, FLD_XTENSA_INFO12, request->buffer_count);
	/* pointer to array of struct vpu_buffer */
	vpu_write_field(core, FLD_XTENSA_INFO13,
				vpu_service_cores[core].work_buf->pa);
	/* pointer to property buffer */
	vpu_write_field(core, FLD_XTENSA_INFO14, request->sett.sett_ptr);
	/* size of property buffer */
	vpu_write_field(core, FLD_XTENSA_INFO15, request->sett.sett_lens);

	/* 2. trigger interrupt */
#ifdef ENABLE_PMQOS
	/* pmqos, 10880 Mbytes per second */
	/*
	 * mtk_pm_qos_update_request(&vpu_qos_bw_request[core],
	 *				request->power_param.bw);
	 */
#endif
	vpu_trace_begin("[vpu_%d] dsp:d2d running", core);
	LOG_DBG("[vpu_%d] d2d running...\n", core);
	#if defined(VPU_MET_READY)
	MET_Events_Trace(1, core, request->algo_id[core]);
	#endif

	vpu_cmd_qos_start(core);

	/* RUN_STALL pull down */
	vpu_write_field(core, FLD_RUN_STALL, 0);
	vpu_write_field(core, FLD_CTL_INT, 1);
	ktime_get_ts(&start);
	if (g_vpu_log_level > Log_ALGO_OPP_INFO)
		LOG_INF("[0x%lx]:d2d start ",
		(unsigned long)request->request_id);
	/* 3. wait until done */
	ret = wait_command(core);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	/* handle for err/exception isr */
	if (exception_isr_check[core])
		ret |= vpu_check_postcond(core);

	if (g_vpu_log_level > VpuLogThre_PERFORMANCE) {
		LOG_INF("[vpu_%d] end d2d, done(%d), ret(%d), info00(%d), %d\n",
			core,
			vpu_service_cores[core].is_cmd_done, ret,
			vpu_read_field(core, FLD_XTENSA_INFO00),
			exception_isr_check[core]);
	}

	if (ret) {
		exception_isr_check[core] = false;
		LOG_ERR("[vpu_%d] hw_d2d err, status(%d/%d), ret(%d), %d\n",
			core,
			vpu_read_field(core, FLD_XTENSA_INFO00),
			vpu_service_cores[core].is_cmd_done,
			ret, exception_isr_check[core]);
		vvpu_vmdla_vcore_checker();
	} else {
		LOG_DBG("[vpu_%d] temp test2.2\n", core);

		/* RUN_STALL pull up to avoid fake cmd */
		vpu_write_field(core, FLD_RUN_STALL, 1);
	}
	ktime_get_ts(&end);
	latency += (uint64_t)(timespec_to_ns(&end) -
		timespec_to_ns(&start));
if (g_vpu_log_level > Log_STATE_MACHINE) {
	LOG_INF("[0x%lx]:load algo + d2d latency[%lld] ",
		(unsigned long)request->request_id, latency);
}
	#ifdef ENABLE_PMQOS
	/* pmqos, release request after d2d done */
	/*
	 * mtk_pm_qos_update_request(&vpu_qos_bw_request[core],
	 * PM_QOS_DEFAULT_VALUE);
	 */
	#endif
	vpu_trace_end();
	#if defined(VPU_MET_READY)
	MET_Events_Trace(0, core, request->algo_id[core]);
	#endif
	LOG_DBG("[vpu_%d] hw_d2d test , status(%d/%d), ret(%d)\n",
			core,
			vpu_read_field(core, FLD_XTENSA_INFO00),
			vpu_service_cores[core].is_cmd_done, ret);
	if (ret) {
		request->status = VPU_REQ_STATUS_TIMEOUT;
		vpu_err_hnd(is_hw_fail, core,
			request, "VPU Timeout",
			"vpu%d: D2D Timeout, algo: %s(%d)",
			core,
			algo ? algo->name : "",
			vpu_service_cores[core].current_algo);
		goto out;
	}

	request->status = (vpu_check_postcond(core)) ?
			VPU_REQ_STATUS_FAILURE : VPU_REQ_STATUS_SUCCESS;
out:
	unlock_command(core);
	vpu_trace_end();
out2:
	if (ret) {
		mutex_lock(&(vpu_service_cores[core].state_mutex));
		vpu_service_cores[core].state = VCT_SHUTDOWN;
		mutex_unlock(&(vpu_service_cores[core].state_mutex));
		/* vpu_put_power(core, VPT_IMT_OFF);*/
		/* blocking use same ththread to avoid race between
		 * delayedworkQ and serviceThread
		 */
		mutex_lock(&power_counter_mutex[core]);
		power_counter[core]--;
		mutex_unlock(&power_counter_mutex[core]);
		apu_get_power_info();

		LOG_ERR("[vpu_%d] pr hw error, force shutdown, cnt(%d)\n",
				core, power_counter[core]);

		if (power_counter[core] == 0)
			vpu_shut_down(core);
	} else {
		vpu_put_power(core, VPT_ENQUE_ON);
	}
	LOG_DBG("[vpu] %s - (%d)", __func__, request->status);
	request->busy_time = (uint64_t)latency;
	/*Todo*/
	//request->bandwidth=(uint32_t)get_vpu_bw();
	request->bandwidth = vpu_cmd_qos_end(core);
	LOG_INF("%s: vpu%d: algo: %s(%d), busy: %lld, bw: %d, ret: %d\n",
		__func__, core, algo ? algo->name : "",
		request->algo_id[core],
		request->busy_time, request->bandwidth, ret);
	mutex_unlock(&vpu_dev->sdsp_control_mutex[core]);

	return ret;

}


int vpu_hw_get_algo_info(int core_s, struct vpu_algo *algo)
{
	int ret = 0;
	int port_count = 0;
	int info_desc_count = 0;
	int sett_desc_count = 0;
	unsigned int ofs_ports, ofs_info, ofs_info_descs, ofs_sett_descs;
	int i;
	bool is_hw_fail = true;
	unsigned int core = (unsigned int)core_s;

	vpu_trace_begin("%s(%d)", __func__, algo->id[core]);
	ret = vpu_get_power(core, false);
	if (ret) {
		LOG_ERR("[vpu_%d]fail to get power!\n", core);
		goto out;
	}

	lock_command(core, VPU_CMD_GET_ALGO);
	LOG_DBG("start to get algo, algo_id=%d\n", algo->id[core]);

	ret = vpu_check_precond(core);
	if (ret) {
		LOG_ERR("[vpu_%d]have wrong status before get algo!\n", core);
		goto out;
	}

	ofs_ports = 0;
	ofs_info = sizeof(((struct vpu_algo *)0)->ports);
	ofs_info_descs = ofs_info + algo->info_length;
	ofs_sett_descs = ofs_info_descs +
			sizeof(((struct vpu_algo *)0)->info_descs);

	LOG_INF("[vpu_%d] %s check precond done\n", core, __func__);

	/* 1. write register */
	vpu_write_field(core, FLD_XTENSA_INFO01, VPU_CMD_GET_ALGO);
	vpu_write_field(core, FLD_XTENSA_INFO06,
			vpu_service_cores[core].work_buf->pa + ofs_ports);
	vpu_write_field(core, FLD_XTENSA_INFO07,
			vpu_service_cores[core].work_buf->pa + ofs_info);
	vpu_write_field(core, FLD_XTENSA_INFO08, algo->info_length);
	vpu_write_field(core, FLD_XTENSA_INFO10,
			vpu_service_cores[core].work_buf->pa + ofs_info_descs);
	vpu_write_field(core, FLD_XTENSA_INFO12,
			vpu_service_cores[core].work_buf->pa + ofs_sett_descs);

	/* 2. trigger interrupt */
	vpu_trace_begin("dsp:running");
	LOG_DBG("[vpu] %s running...\n", __func__);

	/* RUN_STALL pull down */
	vpu_write_field(core, FLD_RUN_STALL, 0);
	vpu_write_field(core, FLD_CTL_INT, 1);

	/* 3. wait until done */
	ret = wait_command(core);
	if (ret == -ERESTARTSYS)
		is_hw_fail = false;

	LOG_INF("[vpu_%d] VPU_CMD_GET_ALGO done\n", core);
	vpu_trace_end();
	if (ret) {
		vpu_err_hnd(is_hw_fail, core,
			NULL, "VPU Timeout", "vpu%d: GET_ALGO Timeout: %s(%d)",
			core, algo->name, algo->id[core]);
		goto out;
	}

	/* RUN_STALL pull up to avoid fake cmd */
	vpu_write_field(core, FLD_RUN_STALL, 1);

	/* 4. get the return value */
	port_count = vpu_read_field(core, FLD_XTENSA_INFO05);
	info_desc_count = vpu_read_field(core, FLD_XTENSA_INFO09);
	sett_desc_count = vpu_read_field(core, FLD_XTENSA_INFO11);
	algo->port_count = port_count;
	algo->info_desc_count = info_desc_count;
	algo->sett_desc_count = sett_desc_count;

	LOG_DBG("end of get algo, %s=%d, %s=%d, %s=%d\n",
		"port_count", port_count,
		"info_desc_count", info_desc_count,
		"sett_desc_count", sett_desc_count);

	/* 5. write back data from working buffer */
	memcpy((void *)(uintptr_t)algo->ports,
		    (void *)((uintptr_t)vpu_service_cores[core].work_buf->va +
							ofs_ports),
			sizeof(struct vpu_port) * port_count);

	for (i = 0 ; i < algo->port_count ; i++) {
		LOG_DBG("port %d.. id=%d, name=%s, dir=%d, usage=%d\n",
			i, algo->ports[i].id, algo->ports[i].name,
			algo->ports[i].dir, algo->ports[i].usage);
	}

	memcpy((void *)(uintptr_t)algo->info_ptr,
		     (void *)((uintptr_t)vpu_service_cores[core].work_buf->va +
							ofs_info),
			algo->info_length);

	memcpy((void *)(uintptr_t)algo->info_descs,
		     (void *)((uintptr_t)vpu_service_cores[core].work_buf->va +
							ofs_info_descs),
			sizeof(struct vpu_prop_desc) * info_desc_count);

	memcpy((void *)(uintptr_t)algo->sett_descs,
		     (void *)((uintptr_t)vpu_service_cores[core].work_buf->va +
							ofs_sett_descs),
			sizeof(struct vpu_prop_desc) * sett_desc_count);

	LOG_DBG("end of get algo 2, %s=%d, %s=%d, %s=%d\n",
		"port_count", algo->port_count,
		"info_desc_count", algo->info_desc_count,
		"sett_desc_count", algo->sett_desc_count);

out:
	unlock_command(core);
	if (ret)
		apu_get_power_info();
	vpu_put_power(core, VPT_ENQUE_ON);
	vpu_trace_end();
	LOG_DBG("[vpu] %s -\n", __func__);
	return ret;
}

void vpu_hw_lock(struct vpu_user *user)
{
	/* CHRISTODO */
	int TEMP_CORE = 0;

	if (user->locked)
		LOG_ERR("double locking bug, pid=%d, tid=%d\n",
				user->open_pid, user->open_tgid);
	else {
		mutex_lock(&lock_mutex);
		is_locked = true;
		user->locked = true;
		vpu_get_power(TEMP_CORE, false);
	}
}

void vpu_hw_unlock(struct vpu_user *user)
{
	/* CHRISTODO */
	int TEMP_CORE = 0;

	if (user->locked) {
		vpu_put_power(TEMP_CORE, VPT_ENQUE_ON);
		is_locked = false;
		user->locked = false;
		wake_up_interruptible(&lock_wait);
		mutex_unlock(&lock_mutex);
	} else
		LOG_ERR("should not unlock while unlocked, pid=%d, tid=%d\n",
				user->open_pid, user->open_tgid);
}

int vpu_alloc_shared_memory(struct vpu_shared_memory **shmem,
	struct vpu_shared_memory_param *param)
{
	int ret = 0;

	/* CHRISTODO */
	struct ion_mm_data mm_data;
	struct ion_sys_data sys_data;
	struct ion_handle *handle = NULL;

	*shmem = kzalloc(sizeof(struct vpu_shared_memory), GFP_KERNEL);
	ret = (*shmem == NULL);
	if (ret) {
		LOG_ERR("fail to kzalloc 'struct memory'!\n");
		goto out;
	}

	handle = ion_alloc(ion_client, param->size, 0,
					ION_HEAP_MULTIMEDIA_MASK, 0);
	ret = (handle == NULL) ? -ENOMEM : 0;
	if (ret) {
		LOG_ERR("fail to alloc ion buffer, ret=%d\n", ret);
		goto out;
	}

	(*shmem)->handle = (void *) handle;

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER_EXT;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = VPU_PORT_OF_IOMMU;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 1;
	if (param->fixed_addr) {
		mm_data.config_buffer_param.reserve_iova_start =
							param->fixed_addr;

		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;
	} else {
		/* CHRISTODO, need revise starting address for working buffer*/
		mm_data.config_buffer_param.reserve_iova_start = 0x60000000;
		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;
	}
	ret = ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
					(unsigned long)&mm_data);
	if (ret) {
		LOG_ERR("fail to config ion buffer, ret=%d\n", ret);
		goto out;
	}

	/* map pa */
	LOG_DBG("vpu param->require_pa(%d)\n", param->require_pa);
	if (param->require_pa) {
		sys_data.sys_cmd = ION_SYS_GET_PHYS;
		sys_data.get_phys_param.kernel_handle = handle;
		sys_data.get_phys_param.phy_addr =
			(unsigned long)(VPU_PORT_OF_IOMMU) << 24 |
			ION_FLAG_GET_FIXED_PHYS;
		sys_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
		ret = ion_kernel_ioctl(ion_client, ION_CMD_SYSTEM,
						(unsigned long)&sys_data);
		if (ret) {
			LOG_ERR("fail to get ion phys, ret=%d\n", ret);
			goto out;
		}

		(*shmem)->pa = sys_data.get_phys_param.phy_addr;
		(*shmem)->length = sys_data.get_phys_param.len;
	}

	/* map va */
	if (param->require_va) {
		(*shmem)->va =
			(uint64_t)(uintptr_t)ion_map_kernel(ion_client, handle);
		ret = ((*shmem)->va) ? 0 : -ENOMEM;
		if (ret) {
			LOG_ERR("fail to map va of buffer!\n");
			goto out;
		}
	}

	return 0;

out:
	if (handle)
		ion_free(ion_client, handle);

	if (*shmem) {
		kfree(*shmem);
		*shmem = NULL;
	}

	return ret;
}

void vpu_free_shared_memory(struct vpu_shared_memory *shmem)
{
	struct ion_handle *handle;

	if (shmem == NULL)
		return;

	handle = (struct ion_handle *) shmem->handle;
	if (handle) {
		ion_unmap_kernel(ion_client, handle);
		ion_free(ion_client, handle);
	}

	kfree(shmem);
}

int vpu_dump_vpu_memory(struct seq_file *s)
{
	vpu_dmp_seq(s);
	vpu_dump_image_file(s);

	return 0;
}

int vpu_dump_register(struct seq_file *s)
{
	return 0;
}

int vpu_dump_image_file(struct seq_file *s)
{
	int i, j, id = 1;
	struct vpu_algo_info *algo_info;
	struct vpu_image_header *header;
	int core = 0;

#define LINE_BAR "  +------+-----+--------------------------------+--------\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-6s|%-5s|%-32s|%-8s|%-11s|%-10s|\n",
			"Header", "Id", "Name", "MagicNum", "MVA", "Length");
	vpu_print_seq(s, LINE_BAR);

	header = (struct vpu_image_header *)
		((uintptr_t)vpu_service_cores[core].bin_base +
					(VPU_OFFSET_IMAGE_HEADERS));
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		for (j = 0; j < header[i].algo_info_count; j++) {
			algo_info = &header[i].algo_infos[j];

			vpu_print_seq(s,
			   "  |%-6d|%-5d|%-32s|0x%-6lx|0x%-9lx|0x%-8x|\n",
			   (i + 1),
			   id,
			   algo_info->name,
			   (unsigned long)(algo_info->vpu_core),
			   algo_info->offset - VPU_OFFSET_ALGO_AREA +
			      (uintptr_t)vpu_service_cores[core].algo_data_mva,
			   algo_info->length);

			id++;
		}
	}

	vpu_print_seq(s, LINE_BAR);
#undef LINE_BAR

	return 0;
}

int vpu_dump_mesg(struct seq_file *s)
{
	vpu_dump_mesg_seq(s, 0);
	vpu_dump_mesg_seq(s, 1);

	return 0;
}

int vpu_dump_mesg_seq(struct seq_file *s, int core_s)
{
	char *ptr = NULL;
	char *log_head = NULL;
	char *log_buf;
	char *log_a_pos = NULL;
	bool jump_out = false;
	unsigned int core = (unsigned int)core_s;

	log_buf = (char *)
		((uintptr_t)vpu_service_cores[core].work_buf->va +
					VPU_OFFSET_LOG);

	if (g_vpu_log_level > 8) {
		int i = 0;
		int line_pos = 0;
		char line_buffer[16 + 1] = {0};

		ptr = log_buf;
		vpu_print_seq(s, "VPU_%d Log Buffer:\n", core);
		for (i = 0; i < VPU_SIZE_LOG_BUF; i++, ptr++) {
			line_pos = i % 16;
			if (line_pos == 0)
				vpu_print_seq(s, "\n%07X0h: ", i / 16);

			line_buffer[line_pos] = isascii(*ptr) &&
						(isprint(*ptr) ? *ptr : '.');

			vpu_print_seq(s, "%02X ", *ptr);
			if (line_pos == 15)
				vpu_print_seq(s, " %s", line_buffer);

		}
		vpu_print_seq(s, "\n\n");
	}

	ptr = log_buf;
	log_head = log_buf;

	/* set the last byte to '\0' */
	vpu_print_seq(s, "=== VPU_%d Log Buffer ===\n", core);
	vpu_print_seq(s, "vpu: print dsp log (0x%x):\n",
					(unsigned int)(uintptr_t)log_buf);

    /* in case total log < VPU_SIZE_LOG_SHIFT and there's '\0' */
	*(log_head + VPU_SIZE_LOG_BUF - 1) = '\0';
	vpu_print_seq(s, "%s", ptr+VPU_SIZE_LOG_HEADER);

	ptr += VPU_SIZE_LOG_HEADER;
	log_head = ptr;

	jump_out = false;
	*(log_head + VPU_SIZE_LOG_DATA - 1) = '\n';
	do {
		if ((ptr + VPU_SIZE_LOG_SHIFT) >=
				(log_head + VPU_SIZE_LOG_DATA)) {

			/* last part of log buffer */
			*(log_head + VPU_SIZE_LOG_DATA - 1) = '\0';
			jump_out = true;
		} else {
			log_a_pos = strchr(ptr + VPU_SIZE_LOG_SHIFT, '\n');
			if (log_a_pos == NULL)
				break;
			*log_a_pos = '\0';
		}
		vpu_print_seq(s, "%s\n", ptr);
		ptr = log_a_pos + 1;

		/* incase log_a_pos is at end of string */
		if (ptr >= log_head + VPU_SIZE_LOG_DATA)
			break;
	} while (!jump_out);

	seq_printf(s, "\n======== vpu%d: logbuf @0x%x ========\n",
		core, vpu_service_cores[core].work_buf->pa + VPU_OFFSET_LOG);
	seq_hex_dump(s, "logbuf ", DUMP_PREFIX_OFFSET, 32, 4,
		(void *)(vpu_service_cores[core].work_buf->va + VPU_OFFSET_LOG),
		VPU_SIZE_LOG_BUF, true);

	return 0;
}

int vpu_dump_opp_table(struct seq_file *s)
{
	int i;

#define LINE_BAR "  +-----+----------+----------+------------+-----------+-----------+\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-5s|%-10s|%-10s|%-10s|%-10s|%-12s|%-11s|%-11s|\n",
				"OPP", "VCORE(uV)", "VVPU(uV)",
				"VMDLA(uV)", "DSP(KHz)",
				"IPU_IF(KHz)", "DSP1(KHz)", "DSP2(KHz)");
	vpu_print_seq(s, LINE_BAR);

	for (i = 0; i < opps.count; i++) {
		vpu_print_seq(s,
"  |%-5d|[%d]%-7d|[%d]%-7d|[%d]%-7d|[%d]%-7d|[%d]%-9d|[%d]%-8d|[%d]%-8d|\n",
			i,
			opps.vcore.opp_map[i],
			opps.vcore.values[opps.vcore.opp_map[i]],
			opps.vvpu.opp_map[i],
			opps.vvpu.values[opps.vvpu.opp_map[i]],
			opps.vmdla.opp_map[i],
			opps.vmdla.values[opps.vmdla.opp_map[i]],
			opps.dsp.opp_map[i],
			opps.dsp.values[opps.dsp.opp_map[i]],
			opps.ipu_if.opp_map[i],
			opps.ipu_if.values[opps.ipu_if.opp_map[i]],
			opps.dspcore[0].opp_map[i],
			opps.dspcore[0].values[opps.dspcore[0].opp_map[i]],
			opps.dspcore[1].opp_map[i],
			opps.dspcore[1].values[opps.dspcore[1].opp_map[i]]);
	}

	vpu_print_seq(s, LINE_BAR);
#undef LINE_BAR

	return 0;
}

int vpu_dump_power(struct seq_file *s)
{
	int vvpu_opp = 0;


	vvpu_opp = vpu_get_hw_vvpu_opp(0);


vpu_print_seq(s, "%s(rw): %s[%d/%d]\n",
			"dvfs_debug",
			"vvpu", opps.vvpu.index, vvpu_opp);
vpu_print_seq(s, "%s[%d], %s[%d], %s[%d], %s[%d]\n",
			"dsp", opps.dsp.index,
			"ipu_if", opps.ipu_if.index,
			"dsp1", opps.dspcore[0].index,
			"dsp2", opps.dspcore[1].index);
vpu_print_seq(s, "min/max boost[0][%d/%d], min/max opp[1][%d/%d]\n",
		min_boost[0], max_boost[0], min_boost[1], max_boost[1]);

vpu_print_seq(s, "min/max opp[0][%d/%d], min/max opp[1][%d/%d]\n",
		min_opp[0], max_opp[0], min_opp[1], max_opp[1]);

vpu_print_seq(s, "is_power_debug_lock(rw): %d\n", is_power_debug_lock);

	return 0;
}

int vpu_dump_vpu(struct seq_file *s)
{
	int core;

#define LINE_BAR "  +-------------+------+-------+-------+-------+-------+\n"
	vpu_print_seq(s, LINE_BAR);
	vpu_print_seq(s, "  |%-12s|%-34s|\n",
			"Queue#", "Waiting");
	vpu_print_seq(s, LINE_BAR);

	vpu_print_seq(s, "  |%-12s|%-34d|\n",
			      "Common",
			      vpu_pool_size(&vpu_dev->pool_common));

	for (core = 0 ; core < MTK_VPU_CORE; core++) {
		vpu_print_seq(s, "  |Core %-7d|%-34d|\n",
				      core,
				      vpu_pool_size(&vpu_dev->pool[core]));
	}
	vpu_print_seq(s, "\n");

#undef LINE_BAR

	return 0;
}

int vpu_set_power_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;
	unsigned int lv = 0;

	switch (param) {
	case VPU_POWER_PARAM_FIX_OPP:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		switch (args[0]) {
		case 0:
			is_power_debug_lock = false;
			break;
		case 1:
			is_power_debug_lock = true;
			break;
		default:
			if (ret) {
				LOG_ERR("invalid argument, received:%d\n",
						(int)(args[0]));
				goto out;
			}
			ret = -EINVAL;
			goto out;
		}
		break;
	case VPU_POWER_PARAM_DVFS_DEBUG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		lv = (unsigned int)args[0];
		ret = lv >= opps.count;

		if (ret) {
			LOG_ERR("opp step(%d) is out-of-bound, count:%d\n",
					(int)(args[0]), opps.count);
			goto out;
		}

		opps.vcore.index = opps.vcore.opp_map[lv];
		opps.vvpu.index = opps.vvpu.opp_map[lv];
		opps.vmdla.index = opps.vmdla.opp_map[lv];
		opps.dsp.index = opps.dsp.opp_map[lv];
		opps.ipu_if.index = opps.ipu_if.opp_map[lv];
		opps.dspcore[0].index = opps.dspcore[0].opp_map[lv];
		opps.dspcore[1].index = opps.dspcore[1].opp_map[lv];

		is_power_debug_lock = true;

		break;
	case VPU_POWER_PARAM_JTAG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		is_jtag_enabled = args[0];
		ret = vpu_hw_enable_jtag(is_jtag_enabled);

		break;
	case VPU_POWER_PARAM_LOCK:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		is_power_debug_lock = args[0];

		break;
	case VPU_POWER_HAL_CTL:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:3, received:%d\n",
					argc);
			goto out;
		}

		if (args[0] > MTK_VPU_CORE || args[0] < 1) {
			LOG_ERR("core(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}

		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}
		vpu_lock_power.core = args[0];
		vpu_lock_power.lock = true;
		vpu_lock_power.priority = POWER_HAL;
		vpu_lock_power.max_boost_value = args[2];
		vpu_lock_power.min_boost_value = args[1];
		LOG_INF("[vpu]POWER_HAL_LOCK+core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);
		ret = vpu_lock_set_power(&vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case VPU_EARA_CTL:
	{
		struct vpu_lock_power vpu_lock_power;

		ret = (argc == 3) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:3, received:%d\n",
					argc);
			goto out;
		}
		if (args[0] > MTK_VPU_CORE || args[0] < 1) {
			LOG_ERR("core(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}

		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		if (args[2] > 100 || args[2] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[2]));
			goto out;
		}
		vpu_lock_power.core = args[0];
		vpu_lock_power.lock = true;
		vpu_lock_power.priority = EARA_QOS;
		vpu_lock_power.max_boost_value = args[2];
		vpu_lock_power.min_boost_value = args[1];
		LOG_INF("[vpu]EARA_LOCK+core:%d, maxb:%d, minb:%d\n",
			vpu_lock_power.core, vpu_lock_power.max_boost_value,
				vpu_lock_power.min_boost_value);
		ret = vpu_lock_set_power(&vpu_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
		}
	case VPU_CT_INFO:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}
		apu_dvfs_dump_info();
		break;
	default:
		LOG_ERR("unsupport the power parameter:%d\n", param);
		break;
	}

out:
	return ret;
}
uint8_t vpu_boost_value_to_opp(uint8_t boost_value)
{
	int ret = 0;
	/* set dsp frequency - 0:700 MHz, 1:624 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/

	uint32_t freq = 0;
	uint32_t freq0 = opps.dspcore[0].values[0];
	uint32_t freq1 = opps.dspcore[0].values[1];
	uint32_t freq2 = opps.dspcore[0].values[2];
	uint32_t freq3 = opps.dspcore[0].values[3];
	uint32_t freq4 = opps.dspcore[0].values[4];
	uint32_t freq5 = opps.dspcore[0].values[5];
	uint32_t freq6 = opps.dspcore[0].values[6];
	uint32_t freq7 = opps.dspcore[0].values[7];
	uint32_t freq8 = opps.dspcore[0].values[8];
	uint32_t freq9 = opps.dspcore[0].values[9];
	uint32_t freq10 = opps.dspcore[0].values[10];
	uint32_t freq11 = opps.dspcore[0].values[11];
	uint32_t freq12 = opps.dspcore[0].values[12];
	uint32_t freq13 = opps.dspcore[0].values[13];
	uint32_t freq14 = opps.dspcore[0].values[14];
	uint32_t freq15 = opps.dspcore[0].values[15];

	if ((boost_value <= 100) && (boost_value >= 0))
		freq = boost_value * freq0 / 100;
	else
		freq = freq0;

	if (freq <= freq0 && freq > freq1)
		ret = 0;
	else if (freq <= freq1 && freq > freq2)
		ret = 1;
	else if (freq <= freq2 && freq > freq3)
		ret = 2;
	else if (freq <= freq3 && freq > freq4)
		ret = 3;
	else if (freq <= freq4 && freq > freq5)
		ret = 4;
	else if (freq <= freq5 && freq > freq6)
		ret = 5;
	else if (freq <= freq6 && freq > freq7)
		ret = 6;
	else if (freq <= freq7 && freq > freq8)
		ret = 7;
	else if (freq <= freq8 && freq > freq9)
		ret = 8;
	else if (freq <= freq9 && freq > freq10)
		ret = 9;
	else if (freq <= freq10 && freq > freq11)
		ret = 10;
	else if (freq <= freq11 && freq > freq12)
		ret = 11;
	else if (freq <= freq12 && freq > freq13)
		ret = 12;
	else if (freq <= freq13 && freq > freq14)
		ret = 13;
	else if (freq <= freq14 && freq > freq15)
		ret = 14;
	else
		ret = 15;

	LOG_DVFS("%s opp %d\n", __func__, ret);
	return ret;
}

bool vpu_update_lock_power_parameter(struct vpu_lock_power *vpu_lock_power)
{
	bool ret = true;
	unsigned int i, core = 0xFFFFFFFF;
	unsigned int priority = vpu_lock_power->priority;

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, MTK_VPU_CORE);
		ret = false;
		return ret;
	}
	lock_power[priority][core].core = core;
	lock_power[priority][core].max_boost_value =
		vpu_lock_power->max_boost_value;
	lock_power[priority][core].min_boost_value =
		vpu_lock_power->min_boost_value;
	lock_power[priority][core].lock = true;
	lock_power[priority][core].priority =
		vpu_lock_power->priority;
LOG_INF("power_parameter core %d, maxb:%d, minb:%d priority %d\n",
		lock_power[priority][core].core,
		lock_power[priority][core].max_boost_value,
		lock_power[priority][core].min_boost_value,
		lock_power[priority][core].priority);
	return ret;
}
bool vpu_update_unlock_power_parameter(struct vpu_lock_power *vpu_lock_power)
{
	bool ret = true;
	unsigned int i, core = 0xFFFFFFFF;
	unsigned int priority = vpu_lock_power->priority;

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, MTK_VPU_CORE);
		ret = false;
		return ret;
	}
	lock_power[priority][core].core =
		vpu_lock_power->core;
	lock_power[priority][core].max_boost_value =
		vpu_lock_power->max_boost_value;
	lock_power[priority][core].min_boost_value =
		vpu_lock_power->min_boost_value;
	lock_power[priority][core].lock = false;
	lock_power[priority][core].priority =
		vpu_lock_power->priority;
	LOG_INF("%s\n", __func__);
	return ret;
}
uint8_t min_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value1;
	else
		return value2;
}
uint8_t max_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value2;
	else
		return value1;
}

bool vpu_update_max_opp(struct vpu_lock_power *vpu_lock_power)
{
	bool ret = true;
	unsigned int i, core = 0xFFFFFFFF;
	uint8_t first_priority = NORMAL;
	uint8_t first_priority_max_boost_value = 100;
	uint8_t first_priority_min_boost_value = 0;
	uint8_t temp_max_boost_value = 100;
	uint8_t temp_min_boost_value = 0;
	bool lock = false;
	uint8_t priority = NORMAL;
	uint8_t maxboost = 100;
	uint8_t minboost = 0;

	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, MTK_VPU_CORE);
		ret = false;
		return ret;
	}

	for (i = 0 ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
		if (lock_power[i][core].lock == true
			&& lock_power[i][core].priority < NORMAL) {
			first_priority = i;
			first_priority_max_boost_value =
				lock_power[i][core].max_boost_value;
			first_priority_min_boost_value =
				lock_power[i][core].min_boost_value;
			break;
			}
	}
	temp_max_boost_value = first_priority_max_boost_value;
	temp_min_boost_value = first_priority_min_boost_value;
/*find final_max_boost_value*/
	for (i = first_priority ; i < VPU_OPP_PRIORIYY_NUM ; i++) {
		lock = lock_power[i][core].lock;
		priority = lock_power[i][core].priority;
		maxboost = lock_power[i][core].max_boost_value;
		minboost = lock_power[i][core].min_boost_value;
	if (lock == true
		&& priority < NORMAL &&
			(((maxboost <= temp_max_boost_value)
				&& (maxboost >= temp_min_boost_value))
			|| ((minboost <= temp_max_boost_value)
				&& (minboost >= temp_min_boost_value))
			|| ((maxboost >= temp_max_boost_value)
				&& (minboost <= temp_min_boost_value)))) {

		temp_max_boost_value =
	min_of(temp_max_boost_value, lock_power[i][core].max_boost_value);
		temp_min_boost_value =
	max_of(temp_min_boost_value, lock_power[i][core].min_boost_value);

			}
	}
	max_boost[core] = temp_max_boost_value;
	min_boost[core] = temp_min_boost_value;
	max_opp[core] =
		vpu_boost_value_to_opp(temp_max_boost_value);
	min_opp[core] =
		vpu_boost_value_to_opp(temp_min_boost_value);
LOG_DVFS("final_min_boost_value:%d final_max_boost_value:%d\n",
		temp_min_boost_value, temp_max_boost_value);
	return ret;
}

int vpu_lock_set_power(struct vpu_lock_power *vpu_lock_power)
{
	int ret = -1;
	unsigned int i, core = 0xFFFFFFFF;

	mutex_lock(&power_lock_mutex);
	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, MTK_VPU_CORE);
		ret = -1;
		mutex_unlock(&power_lock_mutex);
		return ret;
	}


	if (!vpu_update_lock_power_parameter(vpu_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	if (!vpu_update_max_opp(vpu_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	mutex_unlock(&power_lock_mutex);
	return 0;
}
int vpu_unlock_set_power(struct vpu_lock_power *vpu_lock_power)
{
	int ret = -1;
	unsigned int i, core = 0xFFFFFFFF;

	mutex_lock(&power_lock_mutex);
	for (i = 0 ; i < MTK_VPU_CORE ; i++) {
		if (vpu_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_VPU_CORE) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			vpu_lock_power->core, core, MTK_VPU_CORE);
		ret = false;
		mutex_unlock(&power_lock_mutex);
		return ret;
	}
	if (!vpu_update_unlock_power_parameter(vpu_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	if (!vpu_update_max_opp(vpu_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	mutex_unlock(&power_lock_mutex);
	return ret;

}


void vpu_lock(int core)
{
	mutex_lock(&vpu_dev->sdsp_control_mutex[core]);
}

void vpu_unlock(int core)
{
	mutex_unlock(&vpu_dev->sdsp_control_mutex[core]);
}


uint32_t vpu_get_iram_data(int core)
{
	if (core < 0 || core >= MTK_VPU_CORE)
		return 0;

	return vpu_service_cores[core].iram_data_mva;
}

struct vpu_shared_memory *vpu_get_kernel_lib(int core)
{
	if (core < 0 || core >= MTK_VPU_CORE)
		return NULL;
	if (!vpu_service_cores[core].exec_kernel_lib)
		return NULL;

	return vpu_service_cores[core].exec_kernel_lib;
}

struct vpu_shared_memory *vpu_get_work_buf(int core)
{
	if (core < 0 || core >= MTK_VPU_CORE)
		return NULL;
	if (!vpu_service_cores[core].work_buf)
		return NULL;

	return vpu_service_cores[core].work_buf;
}

unsigned long vpu_get_ctrl_base(int core)
{
	if (core < 0 || core >= MTK_VPU_CORE)
		return 0;

	return vpu_service_cores[core].vpu_base;
}

