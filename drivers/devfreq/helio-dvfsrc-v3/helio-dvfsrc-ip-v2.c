/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_MTK_WATCHDOG_COMMON
#include <ext_wd_drv.h>
#endif

#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-ip-v2.h>

#include <spm/mtk_vcore_dvfs.h>

#include <linux/mutex.h>

#include <mt-plat/aee.h>
#if defined(CONFIG_MTK_QOS_V2)
#include <mtk_qos_sram.h>
#endif
#if IS_ENABLED(CONFIG_MMPROFILE)
#include <mmprofile.h>
struct dvfsrc_mmp_events_t {
	mmp_event dvfs_event;
	mmp_event level_change;
};
static struct dvfsrc_mmp_events_t dvfsrc_mmp_events;
#endif
static struct regulator *vcore_reg_id;

#define TIME_STAMP_SIZE 40

static DEFINE_SPINLOCK(force_req_lock);
static char	timeout_stamp[TIME_STAMP_SIZE];
static char	force_start_stamp[TIME_STAMP_SIZE];
static char	force_end_stamp[TIME_STAMP_SIZE];
static char sys_stamp[TIME_STAMP_SIZE];
static char opp_forced;

#define dvfsrc_rmw(offset, val, mask, shift) \
	dvfsrc_write(offset, (dvfsrc_read(offset) & ~(mask << shift)) \
			| (val << shift))

int __weak mtk_rgu_cfg_dvfsrc(int enable)
{
	return 0;
}

struct regulator *__weak dvfsrc_vcore_requlator(struct device *dev)
{
	return NULL;
}

static void dvfsrc_set_sw_req(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ3, data, mask, shift);
}

static void dvfsrc_set_sw_req2(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_SW_REQ2, data, mask, shift);
}

static void dvfsrc_set_vcore_request(int data, int mask, int shift)
{
	dvfsrc_rmw(DVFSRC_VCORE_REQUEST, data, mask, shift);
}

static void dvfsrc_get_timestamp(char *p)
{
	int ret = 0;
	u64 sec = local_clock();
	u64 usec = do_div(sec, 1000000000);

	do_div(usec, 1000);
	ret = snprintf(p, TIME_STAMP_SIZE, "%llu.%06llu",
		sec, usec);
	if (ret < 0)
		pr_info("dvfsrc snprintf fail\n");
}

static void dvfsrc_get_sys_stamp(char *p)
{
	u64 sys_time;
	u64 kernel_time;
	int ret = 0;

	kernel_time = sched_clock_get_cyc(&sys_time);
#if defined(CONFIG_MTK_QOS_V2)
	qos_sram_write(DVFSRC_TIMESTAMP_OFFSET,
		kernel_time);
	qos_sram_write(DVFSRC_TIMESTAMP_OFFSET + 0x4,
		kernel_time >> 32);
	qos_sram_write(DVFSRC_TIMESTAMP_OFFSET + 0x8,
		sys_time);
	qos_sram_write(DVFSRC_TIMESTAMP_OFFSET + 0xC,
		sys_time >> 32);
#endif
	if (p) {
		ret = snprintf(p, TIME_STAMP_SIZE, "0x%llx, 0x%llx",
			kernel_time, sys_time);
		if (ret < 0)
			pr_info("dvfsrc snprintf fail\n");
	}
}


static int is_dvfsrc_forced(void)
{
	return opp_forced;
}

int is_dvfsrc_opp_fixed(void)
{
	int ret;
	unsigned long flags;

	if (!is_dvfsrc_enabled())
		return 1;

	if (!(dvfsrc_read(DVFSRC_BASIC_CONTROL) & 0x100))
		return 1;

	if (helio_dvfsrc_flag_get() != 0)
		return 1;

	spin_lock_irqsave(&force_req_lock, flags);
	ret = is_dvfsrc_forced();
	spin_unlock_irqrestore(&force_req_lock, flags);

	return ret;
}

static void dvfsrc_set_force_start(int data)
{
	opp_forced = 1;
	dvfsrc_get_timestamp(force_start_stamp);
	dvfsrc_write(DVFSRC_TARGET_FORCE, data);
	dvfsrc_rmw(DVFSRC_BASIC_CONTROL, 1,
			FORCE_EN_TAR_MASK, FORCE_EN_TAR_SHIFT);
}

static void dvfsrc_set_force_end(void)
{
	dvfsrc_write(DVFSRC_TARGET_FORCE, 0);
}

static void dvfsrc_release_force(void)
{
	dvfsrc_rmw(DVFSRC_BASIC_CONTROL, 0,
			FORCE_EN_TAR_MASK, FORCE_EN_TAR_SHIFT);
	dvfsrc_write(DVFSRC_TARGET_FORCE, 0);
	dvfsrc_get_timestamp(force_end_stamp);
	opp_forced = 0;
}

static void dvfsrc_set_sw_bw(int type, int data)
{
	data = data / 100;

	if (data > 0xFF)
		data = 0xFF;

	switch (type) {
	case DVFSRC_QOS_APU_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_0, data);
		break;
	case DVFSRC_QOS_CPU_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_1, data);
		break;
	case DVFSRC_QOS_GPU_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_2, data);
		break;
	case DVFSRC_QOS_MM_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_3, data);
		break;
	case DVFSRC_QOS_OTHER_MEMORY_BANDWIDTH:
		dvfsrc_write(DVFSRC_SW_BW_4, data);
		break;
	default:
		break;
	}
}

void dvfsrc_enable_level_intr(int en)
{
	if (en == 1)
		dvfsrc_write(DVFSRC_INT_EN,
			dvfsrc_read(DVFSRC_INT_EN) | (0x4));
	else
		dvfsrc_write(DVFSRC_INT_EN,
			dvfsrc_read(DVFSRC_INT_EN) & ~(0x4));
}

int helio_dvfsrc_level_mask_get(void)
{
	return	dvfsrc_read(DVFSRC_LEVEL_MASK);
}

int helio_dvfsrc_level_mask_set(bool en, int level)
{
	if (en)
		dvfsrc_rmw(DVFSRC_LEVEL_MASK, 1, 1, level);
	else
		dvfsrc_rmw(DVFSRC_LEVEL_MASK, 0, 1, level);
	return	0;
}


int get_cur_vcore_dvfs_opp(void)
{
	int val = __builtin_ffs(dvfsrc_read(DVFSRC_CURRENT_LEVEL));

	if (val == 0)
		return VCORE_DVFS_OPP_NUM;
	else
		return val - 1;
}

int commit_data(int type, int data, int check_spmfw)
{
	int ret = 0;
	int level = 16, opp = 16;
	unsigned long flags;
	int opp_uv;
	int vcore_uv;

	if (!is_dvfsrc_enabled())
		return ret;

	if (check_spmfw)
		mtk_spmfw_init(1, 0);

	switch (type) {
	case DVFSRC_QOS_APU_MEMORY_BANDWIDTH:
	case DVFSRC_QOS_CPU_MEMORY_BANDWIDTH:
	case DVFSRC_QOS_GPU_MEMORY_BANDWIDTH:
	case DVFSRC_QOS_OTHER_MEMORY_BANDWIDTH:
	case DVFSRC_QOS_MM_MEMORY_BANDWIDTH:
		if (data < 0)
			data = 0;
		dvfsrc_set_sw_bw(type, data);
		break;
	case DVFSRC_QOS_DDR_OPP:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= DDR_OPP_NUM || data < 0)
			data = DDR_OPP_NUM - 1;

		opp = data;
		level = DDR_OPP_NUM - data - 1;

		dvfsrc_set_sw_req(level, DDR_SW_AP_MASK, DDR_SW_AP_SHIFT);

		if (!is_dvfsrc_forced() && check_spmfw) {
			udelay(1);
			dvfsrc_wait_for_completion(
				dvfsrc_read(DVFSRC_TARGET_LEVEL) == 0,
				DVFSRC_TIMEOUT);
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_ddr_opp() <= opp,
					DVFSRC_TIMEOUT);
		}
		spin_unlock_irqrestore(&force_req_lock, flags);
		break;
	case DVFSRC_QOS_VCORE_OPP:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= VCORE_OPP_NUM)
			data = VCORE_OPP_NUM - 1;

		if (data < 0) {
			pr_info("VCORE OPP = %d\n", data);
			data = 0;
		}

		opp = data;
		level = VCORE_OPP_NUM - data - 1;

		dvfsrc_set_sw_req(level, VCORE_SW_AP_MASK, VCORE_SW_AP_SHIFT);

		if (!is_dvfsrc_forced() && check_spmfw) {
			udelay(1);
			dvfsrc_wait_for_completion(
				dvfsrc_read(DVFSRC_TARGET_LEVEL) == 0,
				DVFSRC_TIMEOUT);
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_opp() <= opp,
					DVFSRC_TIMEOUT);
		}
		spin_unlock_irqrestore(&force_req_lock, flags);
		if (!is_dvfsrc_forced() && check_spmfw) {
			if (vcore_reg_id) {
				vcore_uv = regulator_get_voltage(vcore_reg_id);
				opp_uv = get_vcore_uv_table(opp);
				if (vcore_uv < opp_uv) {
					pr_info("DVFS FAIL= %d %d 0x%08x 0x%08x %08x\n",
					vcore_uv, opp_uv,
					dvfsrc_read(DVFSRC_CURRENT_LEVEL),
					dvfsrc_read(DVFSRC_TARGET_LEVEL),
					get_cur_vcore_dvfs_opp()); /* TODO */

					aee_kernel_warning("DVFSRC",
						"VCORE failed.",
						__func__);
				}
			}
		}
		break;
	case DVFSRC_QOS_SCP_VCORE_REQUEST:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= VCORE_OPP_NUM || data < 0)
			data = 0;

		opp = VCORE_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_vcore_request(level,
				VCORE_SCP_GEAR_MASK, VCORE_SCP_GEAR_SHIFT);

		if (!is_dvfsrc_forced() && check_spmfw) {
			udelay(1);
			dvfsrc_wait_for_completion(
				dvfsrc_read(DVFSRC_TARGET_LEVEL) == 0,
				DVFSRC_TIMEOUT);
			udelay(1);
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_opp() <= opp,
					DVFSRC_TIMEOUT);
		}
		spin_unlock_irqrestore(&force_req_lock, flags);
		break;
	case DVFSRC_QOS_POWER_MODEL_DDR_REQUEST:
		if (data >= DDR_OPP_NUM || data < 0)
			data = 0;

		opp = DDR_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_sw_req2(level,
				DDR_SW_AP_MASK, DDR_SW_AP_SHIFT);
		break;
	case DVFSRC_QOS_POWER_MODEL_VCORE_REQUEST:
		if (data >= VCORE_OPP_NUM || data < 0)
			data = 0;

		opp = VCORE_OPP_NUM - data - 1;
		level = data;

		dvfsrc_set_sw_req2(level,
				VCORE_SW_AP_MASK, VCORE_SW_AP_SHIFT);
		break;
	case DVFSRC_QOS_VCORE_DVFS_FORCE_OPP:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data >= VCORE_DVFS_OPP_NUM || data < 0)
			data = VCORE_DVFS_OPP_NUM;

		opp = data;
		if (opp == VCORE_DVFS_OPP_NUM) {
			dvfsrc_release_force();
			spin_unlock_irqrestore(&force_req_lock, flags);
			break;
		}

		level = opp;
		dvfsrc_set_force_start(1 << level);
		if (check_spmfw) {
			ret = dvfsrc_wait_for_completion(
					get_cur_vcore_dvfs_opp() == opp,
					DVFSRC_TIMEOUT);
		}
		dvfsrc_set_force_end();
		spin_unlock_irqrestore(&force_req_lock, flags);
		break;
	case DVFSRC_QOS_ISP_HRT_BANDWIDTH:
		spin_lock_irqsave(&force_req_lock, flags);
		if (data < 0)
			data = 0;

		dvfsrc_set_isp_hrt_bw(data);
		opp = dvfsrc_calc_isp_hrt_opp(data);
		if (!is_dvfsrc_forced() && check_spmfw) {
			udelay(1);
			dvfsrc_wait_for_completion(
				dvfsrc_read(DVFSRC_TARGET_LEVEL) == 0,
				DVFSRC_TIMEOUT);
			ret = dvfsrc_wait_for_completion(
				get_cur_ddr_opp() <= opp,
				DVFSRC_TIMEOUT);
		}
		spin_unlock_irqrestore(&force_req_lock, flags);
		break;
	default:
		break;
	}

	if (!(dvfsrc_read(DVFSRC_BASIC_CONTROL) & 0x100)) {
		pr_info("DVFSRC OUT Disable\n");
		return ret;
	}

	if (ret < 0) {
		pr_info("%s: type: 0x%x, data: 0x%x, opp: %d, level: %d\n",
				__func__, type, data, opp, level);
		dvfsrc_dump_reg(NULL, 0);
		aee_kernel_warning("DVFSRC", "%s: failed.", __func__);
	}

	return ret;
}

static void dvfsrc_level_change_dump(void)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_log_ex(dvfsrc_mmp_events.level_change,
		MMPROFILE_FLAG_PULSE, dvfsrc_read(DVFSRC_CURRENT_LEVEL),
		dvfsrc_read(DVFSRC_TARGET_LEVEL));
#endif
}
static irqreturn_t helio_dvfsrc_interrupt(int irq, void *dev_id)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_INT);
	dvfsrc_write(DVFSRC_INT_CLR, val);
	dvfsrc_write(DVFSRC_INT_CLR, 0x0);
	if (val & 0x2) {
		dvfsrc_write(DVFSRC_INT_EN,
			dvfsrc_read(DVFSRC_INT_EN) & ~(0x2));
		dvfsrc_get_timestamp(timeout_stamp);
	}

	if (val & 0x4)
		dvfsrc_level_change_dump();

	return IRQ_HANDLED;
}

static int dvfsrc_resume(struct helio_dvfsrc *dvfsrc)
{
	dvfsrc_get_sys_stamp(sys_stamp);
#ifdef DVFSRC_SUSPEND_SUPPORT
	dvfsrc_resume_cb(dvfsrc);
#endif
	return 0;
}

static int dvfsrc_suspend(struct helio_dvfsrc *dvfsrc)
{
#ifdef DVFSRC_SUSPEND_SUPPORT
	dvfsrc_suspend_cb(dvfsrc);
#endif
	return 0;
}

int get_sw_req_vcore_opp(void)
{
	int opp = -1;
	int sw_req = -1;
	int scp_req = -1;

	/* return opp 0, if dvfsrc not enable */
	if (!is_dvfsrc_enabled())
		return 0;
	/* 1st get sw req opp  no lock protect is ok*/
	if (!is_dvfsrc_forced()) {
		sw_req = (dvfsrc_read(DVFSRC_SW_REQ3) >> VCORE_SW_AP_SHIFT);
		sw_req = sw_req & VCORE_SW_AP_MASK;
		sw_req = VCORE_OPP_NUM - sw_req - 1;
		if (vcorefs_get_scp_req_status()) {
			scp_req = ((dvfsrc_read(DVFSRC_VCORE_REQUEST)
				>> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK);
			scp_req = VCORE_OPP_NUM - scp_req - 1;
		}
		/* return sw_request, as vcore floor level*/
		return (sw_req > scp_req) ? scp_req : sw_req;
	}
	opp = get_cur_vcore_opp();
	return opp; /* return opp , as vcore fixed level*/
}

int helio_dvfsrc_config(struct helio_dvfsrc *dvfsrc)
{
	struct platform_device *pdev = to_platform_device(dvfsrc->dev);
	int ret;

	vcore_reg_id = dvfsrc_vcore_requlator(&pdev->dev);
	if (!vcore_reg_id)
		pr_info("[DVFSRC] No Vcore regulator\n");

	dvfsrc_get_sys_stamp(sys_stamp);
#ifdef CONFIG_MTK_WATCHDOG_COMMON
	dvfsrc_latch_register(1);
#endif
	helio_dvfsrc_enable(1);
	helio_dvfsrc_platform_init(dvfsrc);

#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_enable(1);
	if (dvfsrc_mmp_events.dvfs_event == 0) {
		dvfsrc_mmp_events.dvfs_event = mmprofile_register_event(
			MMP_ROOT_EVENT, "VCORE_DVFS");
		dvfsrc_mmp_events.level_change =  mmprofile_register_event(
			dvfsrc_mmp_events.dvfs_event, "level_change");
		mmprofile_enable_event_recursive(
			dvfsrc_mmp_events.dvfs_event, 1);
	}
	mmprofile_start(1);
#endif

	dvfsrc->irq = platform_get_irq(pdev, 0);
	ret = request_irq(dvfsrc->irq, helio_dvfsrc_interrupt
		, IRQF_TRIGGER_NONE, "dvfsrc", dvfsrc);
	if (ret)
		pr_info("dvfsrc interrupt no use\n");

	dvfsrc->resume = dvfsrc_resume;
	dvfsrc->suspend = dvfsrc_suspend;

	return 0;
}

/*  FOR DEBUGGUNG */
u32 vcorefs_get_md_scenario(void)
{
	return dvfsrc_read(DVFSRC_DEBUG_STA_0);
}
EXPORT_SYMBOL(vcorefs_get_md_scenario);

u32 vcorefs_get_total_emi_status(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_2);

	val = (val >> DEBUG_STA2_EMI_TOTAL_SHIFT) & DEBUG_STA2_EMI_TOTAL_MASK;

	return val;
}

u32 vcorefs_get_scp_req_status(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_2);

	val = (val >> DEBUG_STA2_SCP_SHIFT) & DEBUG_STA2_SCP_MASK;

	return val;
}

u32 vcorefs_get_md_emi_latency_status(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_2);

	val = (val >> DEBUG_STA2_MD_EMI_LATENCY_SHIFT)
		& DEBUG_STA2_MD_EMI_LATENCY_MASK;

	return val;
}

u32 vcorefs_get_hifi_scenario(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_2);

	val = (val >> DEBUG_STA2_HIFI_SCENARIO_SHIFT)
		& DEBUG_STA2_HIFI_SCENARIO_MASK;

	return val;
}

u32 vcorefs_get_hifi_vcore_status(void)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(vcorefs_get_hifi_scenario());

	if (hifi_scen)
		return (dvfsrc_read(DVFSRC_VCORE_REQUEST4) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;
}

u32 vcorefs_get_hifi_ddr_status(void)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(vcorefs_get_hifi_scenario());

	if (hifi_scen)
		return (dvfsrc_read(DVFSRC_DDR_REQUEST6) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;
}
#if defined(DVFSRC_IP_V2_1) || defined(DVFSRC_IP_V2_2)
u32 dvfsrc_get_md_bw(void)
{
#if defined(DVFSRC_IP_V2_1)
	u32 last = dvfsrc_read(DVFSRC_LAST);
#endif
	u32 is_turbo, is_urgent, md_scen;
	u32 val;

#if defined(DVFSRC_IP_V2_1)
	val = dvfsrc_read(DVFSRC_RECORD_0_6 + RECORD_SHIFT * last);
	is_turbo =
		(val >> MD_TURBO_SWITCH_SHIFT) & MD_TURBO_SWITCH_MASK;
#else
	val = dvfsrc_read(DVFSRC_DEBUG_STA_0);
	is_turbo =
		(val >> DEBUG_MDTURBO_SHIFT) & DEBUG_MDTURBO_MASK;
#endif

	val = dvfsrc_read(DVFSRC_DEBUG_STA_0);
	is_urgent =
		(val >> MD_EMI_URG_DEBUG_SHIFT) & MD_EMI_URG_DEBUG_MASK;

	md_scen =
		(val >> MD_EMI_VAL_DEBUG_SHIFT) & MD_EMI_VAL_DEBUG_MASK;

	if (is_urgent) {
		val = dvfsrc_read(DVSFRC_HRT_REQ_MD_URG);
		if (is_turbo) {
			val = (val >> MD_HRT_BW_URG1_SHIFT)
				& MD_HRT_BW_URG1_MASK;
		} else {
			val = (val >> MD_HRT_BW_URG_SHIFT)
				& MD_HRT_BW_URG_MASK;
		}
	} else {
		u32 index, shift;

		index = md_scen / 3;
		shift = (md_scen % 3) * 10;

		if (index > 10)
			return 0;

		if (index < 8) {
			if (is_turbo)
				val = dvfsrc_read(DVFSRC_HRT1_REQ_MD_BW_0
					+ index * 4);
			else
				val = dvfsrc_read(DVFSRC_HRT_REQ_MD_BW_0
					+ index * 4);
		} else {
			if (is_turbo)
				val = dvfsrc_read(DVFSRC_HRT1_REQ_MD_BW_8
					+ (index - 8) * 4);
			else
				val = dvfsrc_read(DVFSRC_HRT_REQ_MD_BW_8
					+ (index - 8) * 4);
		}
		val = (val >> shift) & MD_HRT_BW_MASK;
	}
	return val;
}
#else
u32 dvfsrc_get_md_bw(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_3);
	val = (val >> DEBUG_STA3_MD_HRT_BW_SHIFT)
		& DEBUG_STA3_MD_HRT_BW_MASK;

	return val;
}
#endif

#if defined(DVFSRC_IP_V2_1)
u32 vcorefs_get_hrt_bw_ddr(void)
{
	u32 last, val;

	last = dvfsrc_read(DVFSRC_LAST);
	val = dvfsrc_read(DVFSRC_RECORD_0_6 + RECORD_SHIFT * last);
	val = (val >> RECORD_HRT_BW_REQ_SHIFT) & RECORD_HRT_BW_REQ_MASK;
	return val;
}
#elif defined(DVFSRC_IP_V2_2)
u32 vcorefs_get_hrt_bw_ddr(void)
{
	u32 last, val;

	last = dvfsrc_read(DVFSRC_LAST);
	val = dvfsrc_read(DVFSRC_RECORD_0_5 + RECORD_SHIFT * last);
	val = (val >> RECORD_HRT_BW_REQ_SHIFT) & RECORD_HRT_BW_REQ_MASK;
	return val;
}
#else
u32 vcorefs_get_hrt_bw_ddr(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_4);
	val = (val >> DEBUG_STA4_HRT_BW_REQ_SHIFT)
		& DEBUG_STA4_HRT_BW_REQ_MASK;

	return val;
}

u32 vcorefs_get_md_imp_ddr(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_4);
	val = (val >> MD_EMI_MD_IMP_SHIFT)
		& MD_EMI_MD_IMP_MASK;

	return val;
}

#endif

#if defined(DVFSRC_IP_V2_1)
u32 vcorefs_get_md_rising_ddr(void)
{
	u32 last, val;

	last = dvfsrc_read(DVFSRC_LAST);
	val = dvfsrc_read(DVFSRC_RECORD_0_6 + RECORD_SHIFT * last);
	val = (val >> RECORD_MD_DDR_LATENCY_REQ) &
		RECORD_MD_DDR_LATENCY_MASK;

	return val;
}

u32 vcorefs_get_hifi_rising_ddr(void)
{
	u32 last, val;

	last = dvfsrc_read(DVFSRC_LAST);
	val = dvfsrc_read(DVFSRC_RECORD_0_6 + RECORD_SHIFT * last);
	val = (val >> RECORD_HIFI_DDR_LATENCY_REQ) &
		RECORD_HIFI_DDR_LATENCY_MASK;

	return val;
}
#else
u32 vcorefs_get_md_rising_ddr(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_0);
	val = (val >> DEBUG_MD_RIS_DDR_SHIFT)
		& DEBUG_MD_RIS_DDR_MASK;

	return val;
}

u32 vcorefs_get_hifi_rising_ddr(void)
{
	u32 val;

	val = dvfsrc_read(DVFSRC_DEBUG_STA_0);
	val = (val >> DEBUG_HIFI_RIS_DDR_SHIFT) &
		DEBUG_HIFI_RIS_DDR_MASK;

	return val;
}

u32 vcorefs_get_md_scenario_ddr(void)
{
	u32 is_turbo, is_urgent, md_scen;
	u32 sta0;
	u32 val;

	is_turbo =
		(dvfsrc_read(DVFSRC_MD_TURBO) == 0) ? 1 : 0;

	sta0 = dvfsrc_read(DVFSRC_DEBUG_STA_0);
	is_urgent =
		(sta0 >> MD_EMI_URG_DEBUG_SHIFT) & MD_EMI_URG_DEBUG_MASK;
	md_scen =
		(sta0 >> MD_EMI_VAL_DEBUG_SHIFT) & MD_EMI_VAL_DEBUG_MASK;

	if (is_urgent)
		val = is_turbo ? dvfsrc_read(DVFSRC_95MD_SCEN_BW4) : 0;
	else {
		u32 index, shift;

		index = md_scen / 8;
		shift = (md_scen % 8) * 4;

		if (md_scen > 31)
			return 0;

		if (is_turbo)
			val = dvfsrc_read(DVFSRC_95MD_SCEN_BW0_T
			+ index * 4);
		else
			val = dvfsrc_read(DVFSRC_95MD_SCEN_BW0
					+ index * 4);

		val = (val >> shift) & DDR_SW_AP_MASK;
	}

	return val;
}
#endif

void get_dvfsrc_reg(char *p)
{
	char timestamp[TIME_STAMP_SIZE];

	dvfsrc_get_timestamp(timestamp);

	p += sprintf(p, "%-16s: 0x%08x\n",
			"BASIC_CONTROL",
			dvfsrc_read(DVFSRC_BASIC_CONTROL));
	p += sprintf(p,
	"%-16s: %08x, %08x, %08x, %08x\n",
			"SW_REQ 1~4",
			dvfsrc_read(DVFSRC_SW_REQ1),
			dvfsrc_read(DVFSRC_SW_REQ2),
			dvfsrc_read(DVFSRC_SW_REQ3),
			dvfsrc_read(DVFSRC_SW_REQ4));
	p += sprintf(p,
	"%-16s: %08x, %08x, %08x, %08x\n",
			"SW_REQ 5~8",
			dvfsrc_read(DVFSRC_SW_REQ5),
			dvfsrc_read(DVFSRC_SW_REQ6),
			dvfsrc_read(DVFSRC_SW_REQ7),
			dvfsrc_read(DVFSRC_SW_REQ8));
	p += sprintf(p, "%-16s: %d, %d, %d, %d, %d\n",
			"SW_BW_0~4",
			dvfsrc_read(DVFSRC_SW_BW_0),
			dvfsrc_read(DVFSRC_SW_BW_1),
			dvfsrc_read(DVFSRC_SW_BW_2),
			dvfsrc_read(DVFSRC_SW_BW_3),
			dvfsrc_read(DVFSRC_SW_BW_4));
#if !defined(DVFSRC_IP_V2_1) && !defined(DVFSRC_IP_V2_2)
	p += sprintf(p, "%-16s: %d, %d\n",
			"SW_BW_5~6",
			dvfsrc_read(DVFSRC_SW_BW_5),
			dvfsrc_read(DVFSRC_SW_BW_6));
#endif
	p += sprintf(p, "%-16s: 0x%08x\n",
			"ISP_HRT",
			dvfsrc_read(DVFSRC_ISP_HRT));
	p += sprintf(p, "%-16s: 0x%08x, 0x%08x, 0x%08x\n",
			"DEBUG_STA",
			dvfsrc_read(DVFSRC_DEBUG_STA_0),
			dvfsrc_read(DVFSRC_DEBUG_STA_1),
			dvfsrc_read(DVFSRC_DEBUG_STA_2));
	p += sprintf(p, "%-16s: 0x%08x\n",
			"DVFSRC_INT",
			dvfsrc_read(DVFSRC_INT));
	p += sprintf(p, "%-16s: 0x%08x\n",
			"DVFSRC_INT_EN",
			dvfsrc_read(DVFSRC_INT_EN));
	p += sprintf(p, "%-16s: 0x%02x\n",
			"TOTAL_EMI_REQ",
			vcorefs_get_total_emi_status());
	p += sprintf(p, "%-16s: %d\n",
			"DDR_QOS_REQ",
			dvfsrc_get_ddr_qos());
	p += sprintf(p, "%-16s: %d\n",
			"HIFI_VCORE_REQ",
			vcorefs_get_hifi_vcore_status());
	p += sprintf(p, "%-16s: %d\n",
			"HIFI_DDR_REQ",
			vcorefs_get_hifi_ddr_status());
	p += sprintf(p, "%-16s: 0x%08x\n",
			"MD_HRT_BW",
			dvfsrc_get_md_bw());
	p += sprintf(p, "%-16s: %d\n",
			"HIFI_RISINGREQ",
			vcorefs_get_hifi_rising_ddr());
	p += sprintf(p, "%-16s: %d\n",
			"MD_RISING_REQ",
			vcorefs_get_md_rising_ddr());
#if !defined(DVFSRC_IP_V2_1) && !defined(DVFSRC_IP_V2_2)
	p += sprintf(p, "%-16s: %d\n",
		"HRT_BW_REQ",
		vcorefs_get_hrt_bw_ddr());
#endif
#if !defined(DVFSRC_IP_V2_1)
	p += sprintf(p, "%-16s: %d\n",
			"MD_SCEN_DDR",
			vcorefs_get_md_scenario_ddr());
#endif

	p += sprintf(p, "%-16s: %d , 0x%08x\n",
			"SCP_VCORE_REQ",
			vcorefs_get_scp_req_status(),
			dvfsrc_read(DVFSRC_VCORE_REQUEST));
	p += sprintf(p, "%-16s: 0x%08x\n",
			"CURRENT_LEVEL",
			dvfsrc_read(DVFSRC_CURRENT_LEVEL));
	p += sprintf(p, "%-16s: 0x%08x\n",
			"TARGET_LEVEL",
			dvfsrc_read(DVFSRC_TARGET_LEVEL));
	p += sprintf(p, "%-16s: %.40s\n",
			"Current Tstamp", timestamp);
	p += sprintf(p, "%-16s: %.40s\n",
			"ForceS Tstamp", force_start_stamp);
	p += sprintf(p, "%-16s: %.40s\n",
			"ForceE Tstamp", force_end_stamp);
	p += sprintf(p, "%-16s: %.40s\n",
			"Timeout Tstamp", timeout_stamp);
	p += sprintf(p, "%-16s: %.40s\n",
			"Sys Tstamp", sys_stamp);
}

void get_dvfsrc_record(char *p)
{
	int i, debug_reg;

	p += sprintf(p, "%-17s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(DVFSRC_LAST));

	for (i = 0; i < 8; i++) {
		debug_reg = DVFSRC_RECORD_0_0 + (i * RECORD_SHIFT);
		p += sprintf(p, "[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 0~3",
			dvfsrc_read(debug_reg),
			dvfsrc_read(debug_reg + 0x4),
			dvfsrc_read(debug_reg + 0x8),
			dvfsrc_read(debug_reg + 0xC));
#if defined(DVFSRC_IP_V2_1)
		p += sprintf(p, "[%d]%-14s: %08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~6",
			dvfsrc_read(debug_reg + 0x10),
			dvfsrc_read(debug_reg + 0x14),
			dvfsrc_read(debug_reg + 0x18));
#else
		p += sprintf(p, "[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~7",
			dvfsrc_read(debug_reg + 0x10),
			dvfsrc_read(debug_reg + 0x14),
			dvfsrc_read(debug_reg + 0x18),
			dvfsrc_read(debug_reg + 0x1C));
#endif
	}
}


/* met profile function */
/* met profile table */
static unsigned int met_vcorefs_info[INFO_MAX];
static char *met_info_name[INFO_MAX] = {
	"OPP",
	"FREQ",
	"VCORE",
	"x__SPM_LEVEL",
};

int vcorefs_get_num_opp(void)
{
	return VCORE_DVFS_OPP_NUM;
}
EXPORT_SYMBOL(vcorefs_get_num_opp);

int vcorefs_get_opp_info_num(void)
{
	return INFO_MAX;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_num);


char **vcorefs_get_opp_info_name(void)
{
	return met_info_name;
}
EXPORT_SYMBOL(vcorefs_get_opp_info_name);

unsigned int *vcorefs_get_opp_info(void)
{
	met_vcorefs_info[INFO_OPP_IDX] = get_cur_vcore_dvfs_opp();
	met_vcorefs_info[INFO_FREQ_IDX] = get_cur_ddr_khz();
	met_vcorefs_info[INFO_VCORE_IDX] = get_cur_vcore_uv();
	met_vcorefs_info[INFO_SPM_LEVEL_IDX] = get_cur_vcore_dvfs_opp();

	return met_vcorefs_info;
}
EXPORT_SYMBOL(vcorefs_get_opp_info);

