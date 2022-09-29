// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#if IS_ENABLED(CONFIG_MTK_PBM)
#include "mtk_pbm.h"
#include "mtk_mdpm.h"
#endif
#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "md_sys1_platform.h"
#include "modem_secure_base.h"

#include "ccci_debug.h"
#include "hif/ccci_hif_cldma.h"
//#include "hif/ccci_hif_ccif.h"
#include "modem_sys.h"
#include "ccci_fsm_sys.h"

#define TAG "mcd"

struct ccci_modem *modem_sys;

#ifdef CCCI_KMODULE_ENABLE
bool spm_is_md1_sleep(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
		MD_GET_SLEEP_MODE, 0, 0, 0, 0, 0, &res);

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] flag_1=%llx, flag_2=%llx, flag_3=%llx, flag_4=%llx\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
	if (res.a0 == 0)
		return res.a1; /* 1-md is sleep, 0 - is not */
	else
		return 2; /* not support, no wait */
}

#endif

void ccif_enable_irq(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (atomic_cmpxchg(&md_info->ccif_irq_enabled, 0, 1) == 0) {
		enable_irq(md_info->ap_ccif_irq_id);
		CCCI_NORMAL_LOG(0, TAG, "enable ccif irq\n");
	}
}

void ccif_disable_irq(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (atomic_cmpxchg(&md_info->ccif_irq_enabled, 1, 0) == 1) {
		disable_irq_nosync(md_info->ap_ccif_irq_id);
		/*CCCI_NORMAL_LOG(0, TAG, "disable ccif irq\n");*/
	}
}

void wdt_enable_irq(struct ccci_modem *md)
{
	if (atomic_cmpxchg(&md->wdt_enabled, 0, 1) == 0) {
		enable_irq(md->md_wdt_irq_id);
		CCCI_NORMAL_LOG(0, TAG, "enable wdt irq\n");
	}
}

void wdt_disable_irq(struct ccci_modem *md)
{
	if (atomic_cmpxchg(&md->wdt_enabled, 1, 0) == 1) {
		/*
		 * may be called in isr, so use disable_irq_nosync.
		 * if use disable_irq in isr, system will hang
		 */
		disable_irq_nosync(md->md_wdt_irq_id);
		/*CCCI_NORMAL_LOG(0, TAG, "disable wdt irq\n");*/
	}
}

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;

	//CCCI_ERROR_LOG(0, TAG, "MD WDT IRQ\n");
	//ccci_event_log("md: MD WDT IRQ\n");
	ccif_disable_irq(md);
	wdt_disable_irq(md);
	ccci_fsm_recv_md_interrupt(MD_IRQ_WDT);

	return IRQ_HANDLED;
}

static void md_cd_exception(struct ccci_modem *md, enum HIF_EX_STAGE stage)
{
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);

	CCCI_ERROR_LOG(0, TAG, "MD exception HIF %d\n", stage);
	ccci_event_log("md:MD exception HIF %d\n", stage);
	/* in exception mode, MD won't sleep, so we do not
	 * need to request MD resource first
	 */
	switch (stage) {
	case HIF_EX_INIT:
		if (*((int *)(mdccci_dbg->base_ap_view_vir +
			CCCI_SMEM_OFFSET_SEQERR)) != 0) {
			CCCI_ERROR_LOG(0, TAG, "MD found wrong sequence number\n");
		}
		ccci_hif_md_exception(md->hif_flag, stage);
		/* Rx dispatch does NOT depend on queue index
		 * in port structure, so it still can find right port.
		 */
		ccci_hif_send_data(CCIF_HIF_ID, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		break;
	case HIF_EX_CLEARQ_DONE:
		/* give DHL some time to flush data */
		msleep(2000);
		ccci_hif_md_exception(md->hif_flag, stage);
		/* tell MD to reset CLDMA */
		ccci_hif_send_data(CCIF_HIF_ID, H2D_EXCEPTION_CLEARQ_ACK);
		CCCI_NORMAL_LOG(0, TAG, "send clearq_ack to MD\n");
		break;
	case HIF_EX_ALLQ_RESET:
		md->per_md_data.is_in_ee_dump = 1;
		ccci_hif_md_exception(md->hif_flag, stage);
		break;
	default:
		break;
	};
}

static void polling_ready(struct ccci_modem *md, int step)
{
	int cnt = 500; /*MD timeout is 10s*/
	int time_once = 10;
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;

#ifdef CCCI_EE_HS_POLLING_TIME
	cnt = CCCI_EE_HS_POLLING_TIME / time_once;
#endif
	while (cnt > 0) {
		if (md_info->channel_id & (1 << step)) {
			CCCI_DEBUG_LOG(0, TAG,
				"poll RCHNUM %d\n", md_info->channel_id);
			return;
		}
		msleep(time_once);
		cnt--;
	}
	CCCI_ERROR_LOG(0, TAG,
		"poll EE HS timeout, RCHNUM %d\n", md_info->channel_id);
}

static int md_cd_ee_handshake(struct ccci_modem *md, int timeout)
{
	/* seems sometime MD send D2H_EXCEPTION_INIT_DONE and
	 * D2H_EXCEPTION_CLEARQ_DONE together
	 */
	/*polling_ready(md_ctrl, D2H_EXCEPTION_INIT);*/
	md_cd_exception(md, HIF_EX_INIT);
	polling_ready(md, D2H_EXCEPTION_INIT_DONE);
	md_cd_exception(md, HIF_EX_INIT_DONE);

	polling_ready(md, D2H_EXCEPTION_CLEARQ_DONE);
	md_cd_exception(md, HIF_EX_CLEARQ_DONE);

	polling_ready(md, D2H_EXCEPTION_ALLQ_RESET);
	md_cd_exception(md, HIF_EX_ALLQ_RESET);

	return 0;
}

int md_fsm_exp_info(unsigned int channel_id)
{
	struct ccci_modem *md;
	struct md_sys1_info *md_info;

	md = ccci_get_modem();
	if (!md)
		return 0;

	md_info = (struct md_sys1_info *)md->private_data;
	if (channel_id & (1 << D2H_EXCEPTION_INIT)) {
		ccci_fsm_recv_md_interrupt(MD_IRQ_CCIF_EX);
		md_info->channel_id = channel_id & ~(1 << D2H_EXCEPTION_INIT);
		return 0;
	}

	if (channel_id & (1<<AP_MD_PEER_WAKEUP)) {
		__pm_wakeup_event(md_info->peer_wake_lock,
			jiffies_to_msecs(HZ));
		channel_id &= ~(1 << AP_MD_PEER_WAKEUP);
	}

	if (channel_id & (1<<AP_MD_SEQ_ERROR)) {
		CCCI_ERROR_LOG(0, TAG, "MD check seq fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		channel_id &= ~(1 << AP_MD_SEQ_ERROR);
	}

	md_info->channel_id |= channel_id;
	return 0;
}
EXPORT_SYMBOL(md_fsm_exp_info);

static inline int md_sys1_sw_init(struct ccci_modem *md)
{
	int ret;

	CCCI_BOOTUP_LOG(0, TAG,
		"%s, MD_WDT IRQ(%d)\n", __func__, md->md_wdt_irq_id);

	ret = request_irq(md->md_wdt_irq_id, md_cd_wdt_isr,
			md->md_wdt_irq_flags, "MD_WDT", md);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG,
			"request MD_WDT IRQ(%d) error %d\n",
			md->md_wdt_irq_id, ret);
		return ret;
	}
	ret = irq_set_irq_wake(md->md_wdt_irq_id, 1);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"irq_set_irq_wake MD_WDT IRQ(%d) error %d\n",
			md->md_wdt_irq_id, ret);
	/* IRQ is enabled after requested, so call enable_irq after
	 * request_irq will get a unbalance warning
	 */
	return 0;
}

static int md_cd_init(struct ccci_modem *md)
{
	CCCI_INIT_LOG(0, TAG, "CCCI: modem is initializing\n");

	return 0;
}

/* Please delete this function once it can be deleted. */
static int ccci_md_hif_start(struct ccci_modem *md, int stage)
{
	switch (stage) {
	case 1:
		/*enable clk: cldma*/
		//if (md->hw_info->plat_ptr->set_clk_cg)
		//	md->hw_info->plat_ptr->set_clk_cg(md, 1);

		//ccci_hif_set_clk_cg(1 << CLDMA_HIF_ID, 1);

		//if (md->hif_flag & (1 << CLDMA_HIF_ID)) {
			/* 2. clearring buffer, just in case */
			//ccci_hif_clear_all_queue(1 << CLDMA_HIF_ID, OUT);
			//ccci_hif_clear_all_queue(1 << CLDMA_HIF_ID, IN);
			//if (md->hw_info->plat_ptr->cldma_hw_rst)
			//	md->hw_info->plat_ptr->cldma_hw_rst();
			//ccci_hif_hw_reset(1 << CLDMA_HIF_ID);
		//}
		break;

	case 2:
		if (md->hif_flag & (1 << CCIF_HIF_ID))
			ccif_enable_irq(md);
		//if (md->hif_flag & (1 << CLDMA_HIF_ID)) {
			/* 8. start CLDMA */
			//ccci_hif_start(CLDMA_HIF_ID);
		//}
		break;
	default:
		break;
	}
	return 0;
}

static int md_cd_start(struct ccci_modem *md)
{
	int ret = 0;

	if (md->per_md_data.config.setting & MD_SETTING_FIRST_BOOT) {
		if (md->hw_info->plat_ptr->remap_md_reg)
			ret = md->hw_info->plat_ptr->remap_md_reg(md);
		if (ret < 0) {
			CCCI_BOOTUP_LOG(0, TAG,
				"remap fail %d\n", ret);
			goto out;
		}
		md_sys1_sw_init(md);

		//ccci_hif_late_init(md->hif_flag);
		/* init security, as security depends on dummy_char,
		 * which is ready very late.
		 */
		if (md->hw_info->plat_ptr->start_platform) {
			ret = md->hw_info->plat_ptr->start_platform(md);
			if (ret) {
				CCCI_BOOTUP_LOG(0, TAG,
					"power on MD BROM fail %d\n", ret);
				goto out;
			}
		}
		md->per_md_data.config.setting &= ~MD_SETTING_FIRST_BOOT;
	}

	CCCI_BOOTUP_LOG(0, TAG, "modem is starting\n");
	/* 1. load modem image */

	CCCI_BOOTUP_LOG(0, TAG, "modem image ready, bypass load\n");
	ret = ccci_get_md_check_hdr_inf(
		&md->per_md_data.img_info[IMG_MD],
		md->per_md_data.img_post_fix);
	if (ret < 0) {
		CCCI_BOOTUP_LOG(0, TAG, "partition read fail(%d)\n", ret);
		/* goto out; */
	} else
		CCCI_BOOTUP_LOG(0, TAG, "partition read success\n");

	ccci_md_hif_start(md, 1);
	/* 4. power on modem, do NOT touch MD register before this */
	if (md->hw_info->plat_ptr->power_on) {
		ret = md->hw_info->plat_ptr->power_on(md);
		if (ret) {
			CCCI_BOOTUP_LOG(0, TAG,
				"power on MD fail %d\n", ret);
			goto out;
		}
	}
#ifdef SET_EMI_STEP_BY_STAGE
	ccci_set_mem_access_protection_1st_stage(md);
#endif
	/* 5. update mutex */
	atomic_set(&md->reset_on_going, 0);

	md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_AP_REG;

	/* 7. let modem go */
	if (md->hw_info->plat_ptr->let_md_go)
		md->hw_info->plat_ptr->let_md_go(md);
	wdt_enable_irq(md);
	ccci_md_hif_start(md, 2);

	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
 out:
	CCCI_BOOTUP_LOG(0, TAG,
		"modem started %d\n", ret);
	/* used for throttling feature - start */
	/*ccci_modem_boot_count[md->index]++;*/
	/* used for throttling feature - end */
	return ret;
}

static int check_power_off_en(struct ccci_modem *md)
{
	int smem_val = 1;
/*
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);

	smem_val = *((int *)((long)mdss_dbg->base_ap_view_vir +
		md->hw_info->plat_val->offset_epof_md1));
*/
	CCCI_NORMAL_LOG(0, TAG, "share for power off:%x\n", smem_val);
	if (smem_val != 0) {
		CCCI_NORMAL_LOG(0, TAG, "[ccci]enable power off check\n");
		return 1;
	}
	CCCI_NORMAL_LOG(0, TAG, "disable power off check\n");
	return 0;
}

static int md_cd_soft_start(struct ccci_modem *md, unsigned int mode)
{
	if (md->hw_info->plat_ptr->soft_power_on == NULL)
		return -1;
	return md->hw_info->plat_ptr->soft_power_on(md, mode);
}

static int md_cd_soft_stop(struct ccci_modem *md, unsigned int mode)
{
	if (md->hw_info->plat_ptr->soft_power_off == NULL)
		return -1;
	return md->hw_info->plat_ptr->soft_power_off(md, mode);
}

void __weak md1_sleep_timeout_proc(void)
{
	CCCI_DEBUG_LOG(-1, TAG, "No %s\n", __func__);
}

static void debug_in_flight_mode(struct ccci_modem *md)
{
	int count = 0;
	int en_power_check = check_power_off_en(md);
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	int md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;

	count = 5;
	while (spm_is_md1_sleep() == 0) {
		count--;
		if (count == 0) {
			if (en_power_check && atomic_read(&md_dapc_ke_occurred)) {
				CCCI_NORMAL_LOG(0, TAG,
				"MD is not in sleep mode, dump md status!\n");
				atomic_set(&en_flight_timeout, 1);
				CCCI_MEM_LOG_TAG(0, TAG,
				"Dump MD EX log\n");
				if (md_dbg_dump_flag &
					(1 << MD_DBG_DUMP_SMEM)) {
					ccci_util_mem_dump(
					CCCI_DUMP_MEM_DUMP,
					mdccci_dbg->base_ap_view_vir,
					mdccci_dbg->size);
					ccci_util_mem_dump(
					CCCI_DUMP_MEM_DUMP,
					mdss_dbg->base_ap_view_vir,
					mdss_dbg->size);
				}
				if (md->hw_info->plat_ptr->debug_reg)
					md->hw_info->plat_ptr->debug_reg(md, true);

				CCCI_MEM_LOG_TAG(0, TAG, "MD not in sleep, dump mdl2sram\n");
				if (md && md->hw_info && md->hw_info->md_l2sram_base) {
					md_cd_lock_modem_clock_src(1);
					ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
						md->hw_info->md_l2sram_base,
						md->hw_info->md_l2sram_size);
					md_cd_lock_modem_clock_src(0);
				}

	/* cldma_dump_register(CLDMA_HIF_ID);*/
/*
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
				aed_md_exception_api(
				mdss_dbg->base_ap_view_vir,
				mdss_dbg->size, NULL, 0,
				"After AP send EPOF, MD didn't go to sleep in 4 seconds.",
				DB_OPT_DEFAULT);
#endif
*/
			} else
				md1_sleep_timeout_proc();
			break;
		}
		msleep(1000);
	}
}

static int md_cd_pre_stop(struct ccci_modem *md, unsigned int stop_type)
{
	u32 pending;
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	int md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;

	/* 1. mutex check */
	if (atomic_add_return(1, &md->reset_on_going) > 1) {
		CCCI_NORMAL_LOG(0, TAG,
			"One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}

	CCCI_NORMAL_LOG(0, TAG,
		"%s: CCCI modem is resetting\n", __func__);
	/* 2. disable WDT IRQ */
	wdt_disable_irq(md);

	/* only debug in Flight mode */
	if (stop_type == MD_FLIGHT_MODE_ENTER) {
		debug_in_flight_mode(md);
#ifdef CCCI_KMODULE_ENABLE
		pending = 0;
#else
		pending = mt_irq_get_pending(md->md_wdt_irq_id);
#endif
		if (pending) {
			CCCI_NORMAL_LOG(0, TAG, "WDT IRQ occur.");
			CCCI_MEM_LOG_TAG(0, TAG, "Dump MD EX log\n");
			if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
				ccci_util_mem_dump(
					CCCI_DUMP_MEM_DUMP,
					mdccci_dbg->base_ap_view_vir,
					mdccci_dbg->size);
				ccci_util_mem_dump(
					CCCI_DUMP_MEM_DUMP,
					mdss_dbg->base_ap_view_vir,
					mdss_dbg->size);
			}
			if (md->hw_info->plat_ptr->debug_reg)
				md->hw_info->plat_ptr->debug_reg(md, true);
			/* cldma_dump_register(CLDMA_HIF_ID);*/
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			aed_md_exception_api(NULL, 0, NULL, 0,
				"WDT IRQ occur.", DB_OPT_DEFAULT);
#endif
		}
	}

	CCCI_NORMAL_LOG(0, TAG, "Reset when MD state: %d\n",
			ccci_fsm_get_md_state());
	return 0;
}

static int md_cd_stop(struct ccci_modem *md, unsigned int stop_type)
{
	int ret = 0;

	CCCI_NORMAL_LOG(0, TAG,
		"modem is power off, stop_type=%d\n", stop_type);
	ccif_disable_irq(md);
	/* Check EMI before */
	if (md->hw_info->plat_ptr->check_emi_state)
		md->hw_info->plat_ptr->check_emi_state(md, 1);

	/* power off MD */
	if (md->hw_info->plat_ptr->power_off)
		ret = md->hw_info->plat_ptr->power_off(md,
			stop_type == MD_FLIGHT_MODE_ENTER ? 100 : 0);
	CCCI_NORMAL_LOG(0, TAG, "modem is power off done, %d\n", ret);
	/* Check EMI after */
	if (md->hw_info->plat_ptr->check_emi_state)
		md->hw_info->plat_ptr->check_emi_state(md, 0);

	return 0;
}

static void dump_runtime_data_v2(struct ccci_modem *md,
	struct ap_query_md_feature *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(0, TAG,
		"head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = BOOT_INFO; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(0, TAG,
			"feature %u: mask %u, version %u\n",
			i, ap_feature->feature_set[i].support_mask,
			ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(0, TAG,
		"share_memory_support 0x%x\n",
		ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(0, TAG,
		"ap_runtime_data_addr 0x%x\n",
		ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"ap_runtime_data_size 0x%x\n",
		ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"md_runtime_data_addr 0x%x\n",
		ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"md_runtime_data_size 0x%x\n",
		ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_start_addr 0x%x\n",
		ap_feature->set_md_mpu_start_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_total_size 0x%x\n",
		ap_feature->set_md_mpu_total_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"tail_pattern 0x%x\n",
		ap_feature->tail_pattern);
}

static void dump_runtime_data_v2_1(struct ccci_modem *md,
	struct ap_query_md_feature_v2_1 *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(0, TAG,
		"head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = BOOT_INFO; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(0, TAG,
			"feature %u: mask %u, version %u\n",
			i, ap_feature->feature_set[i].support_mask,
			ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(0, TAG,
		"share_memory_support 0x%x\n",
		ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(0, TAG,
		"ap_runtime_data_addr 0x%x\n",
		ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"ap_runtime_data_size 0x%x\n",
		ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"md_runtime_data_addr 0x%x\n",
		ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"md_runtime_data_size 0x%x\n",
		ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_noncached_start_addr 0x%x\n",
		ap_feature->noncached_mpu_start_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_noncached_total_size 0x%x\n",
		ap_feature->noncached_mpu_total_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_cached_start_addr 0x%x\n",
		ap_feature->cached_mpu_start_addr);
	CCCI_BOOTUP_LOG(0, TAG,
		"set_md_mpu_cached_total_size 0x%x\n",
		ap_feature->cached_mpu_total_size);
	CCCI_BOOTUP_LOG(0, TAG,
		"tail_pattern 0x%x\n",
		ap_feature->tail_pattern);
}

#if (MD_GENERATION < 6297)
static void md_cd_smem_sub_region_init(struct ccci_modem *md)
{
	int i;
	int __iomem *addr;
	struct ccci_smem_region *dbm =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DBM);

	/* Region 0, dbm */
	addr = (int __iomem *)dbm->base_ap_view_vir;
	addr[0] = 0x44444444; /* Guard pattern 1 header */
	addr[1] = 0x44444444; /* Guard pattern 2 header */

#ifdef DISABLE_PBM_FEATURE
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM / 4 + 2); i++)
		addr[i] = 0xFFFFFFFF;
#else
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM / 4 + 2); i++)
		addr[i] = 0x00000000;
#endif

	addr[i++] = 0x44444444; /* Guard pattern 1 tail */
	addr[i++] = 0x44444444; /* Guard pattern 2 tail */

#if IS_ENABLED(CONFIG_MTK_PBM)
	addr += CCCI_SMEM_SIZE_DBM_GUARD;
#endif
	init_md_section_level(KR_MD1, addr);
}
#else
static void md_cd_smem_sub_region_init(struct ccci_modem *md)
{
#if IS_ENABLED(CONFIG_MTK_PBM)
	int __iomem *addr;
	struct ccci_smem_region *dbm =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DBM);

	/* Region 0, dbm */
	addr = (int __iomem *)dbm->base_ap_view_vir;
	init_md_section_level(KR_MD1, addr);
#endif
}
#endif

static void config_ap_runtime_data_v2(struct ccci_modem *md,
	struct ap_query_md_feature *ap_feature)
{
	struct ccci_smem_region *runtime_data =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_RUNTIME_DATA);

	ap_feature->head_pattern = AP_FEATURE_QUERY_PATTERN;
	/*AP query MD feature set */

	ap_feature->share_memory_support = INTERNAL_MODEM;
	ap_feature->ap_runtime_data_addr = runtime_data->base_md_view_phy;
	ap_feature->ap_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_addr =
		ap_feature->ap_runtime_data_addr + CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_MD;

	ap_feature->set_md_mpu_start_addr =
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy;
	ap_feature->set_md_mpu_total_size =
		md->mem_layout.md_bank4_noncacheable_total.size;

	/* Set Flag for modem on feature_set[1].version,
	 * specially: [1].support_mask = 0
	 */
	ap_feature->feature_set[1].support_mask = 0;
	/* ver.1: set_md_mpu_total_size =
	 * ap md1 share + md1&md3 share
	 */
	/* ver.0: set_md_mpu_total_size = ap md1 share */
	ap_feature->feature_set[1].version = 1;
	ap_feature->tail_pattern = AP_FEATURE_QUERY_PATTERN;
}

static void config_ap_runtime_data_v2_1(struct ccci_modem *md,
	struct ap_query_md_feature_v2_1 *ap_feature)
{
	struct ccci_smem_region *runtime_data =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_RUNTIME_DATA);

	ap_feature->head_pattern = AP_FEATURE_QUERY_PATTERN;
	/*AP query MD feature set */

	/* to let md know that this is new AP. */
	ap_feature->share_memory_support = MULTI_MD_MPU_SUPPORT;
	ap_feature->ap_runtime_data_addr = runtime_data->base_md_view_phy;
	ap_feature->ap_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_addr =
		ap_feature->ap_runtime_data_addr + CCCI_SMEM_SIZE_RUNTIME_AP;
	ap_feature->md_runtime_data_size = CCCI_SMEM_SIZE_RUNTIME_MD;

	ap_feature->noncached_mpu_start_addr =
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy;
	ap_feature->noncached_mpu_total_size =
		md->mem_layout.md_bank4_noncacheable_total.size;
	ap_feature->cached_mpu_start_addr =
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy;
	ap_feature->cached_mpu_total_size =
		md->mem_layout.md_bank4_cacheable_total.size;

	/* Set Flag for modem on feature_set[1].version,
	 * specially: [1].support_mask = 0
	 */
	ap_feature->feature_set[1].support_mask = 0;
	/* ver.1: set_md_mpu_total_size =
	 * ap md1 share + md1&md3 share
	 */
	/* ver.0: set_md_mpu_total_size = ap md1 share */
	ap_feature->feature_set[1].version = 1;
	ap_feature->tail_pattern = AP_FEATURE_QUERY_PATTERN;
}

static int md_cd_send_runtime_data_v2(struct ccci_modem *md,
	unsigned int tx_ch, unsigned int txqno, int skb_from_pool)
{
	int packet_size;
	struct ap_query_md_feature *ap_rt_data = NULL;
	struct ap_query_md_feature_v2_1 *ap_rt_data_v2_1 = NULL;
	int ret;

	if (md->runtime_version < AP_MD_HS_V2) {
		CCCI_ERROR_LOG(0, TAG,
			"unsupported runtime version %d\n",
			md->runtime_version);
		return -CCCI_ERR_CCIF_INVALID_RUNTIME_LEN;
	}

	if (md->multi_md_mpu_support) {
		packet_size = sizeof(struct ap_query_md_feature_v2_1) +
			sizeof(struct ccci_header);

		ap_rt_data_v2_1 = (struct ap_query_md_feature_v2_1 *)
			ccci_hif_fill_rt_header(CCIF_HIF_ID, packet_size,
			tx_ch, txqno);
		if (!ap_rt_data_v2_1) {
			CCCI_ERROR_LOG(0, TAG, "rt header v2 NULL!\n");
			return -1;
		}

		memset_io(ap_rt_data_v2_1, 0,
			sizeof(struct ap_query_md_feature_v2_1));
		config_ap_runtime_data_v2_1(md, ap_rt_data_v2_1);
		dump_runtime_data_v2_1(md, ap_rt_data_v2_1);
	} else {
		/* infactly, 6292 should not be this else condition */
		packet_size = sizeof(struct ap_query_md_feature) +
			sizeof(struct ccci_header);
		ap_rt_data = (struct ap_query_md_feature *)
			ccci_hif_fill_rt_header(CCIF_HIF_ID, packet_size,
			tx_ch, txqno);
		if (!ap_rt_data) {
			CCCI_ERROR_LOG(0, TAG, "rt header NULL!\n");
			return -1;
		}
		memset_io(ap_rt_data, 0, sizeof(struct ap_query_md_feature));
		config_ap_runtime_data_v2(md, ap_rt_data);
		dump_runtime_data_v2(md, ap_rt_data);
	}

	md_cd_smem_sub_region_init(md);

	ret = ccci_hif_send_data(CCIF_HIF_ID, H2D_SRAM);
	return ret;
}

static int md_cd_force_assert(struct ccci_modem *md, enum MD_COMM_TYPE type)
{
	CCCI_NORMAL_LOG(0, TAG,
		"force assert MD using %d\n", type);
	if (type == CCIF_INTERRUPT)
		ccci_hif_send_data(CCIF_HIF_ID, AP_MD_SEQ_ERROR);
	else if (type == CCIF_MPU_INTR) {
		ccci_hif_send_data(CCIF_HIF_ID, H2D_MPU_FORCE_ASSERT);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG, NULL, 0);
	}
	return 0;
}

static int md_cd_dump_info(struct ccci_modem *md,
	enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	if (flag & DUMP_FLAG_CCIF_REG) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump CCIF REG\n");
		ccci_hif_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF_REG,
			NULL, -1);
	}
	if (flag & DUMP_FLAG_PCCIF_REG) {
		CCCI_MEM_LOG_TAG(0, TAG, "ignore Dump PCCIF REG\n");
		/*md_cd_dump_pccif_reg(md);*/
	}
	if (flag & DUMP_FLAG_CCIF) {
		unsigned int *dest_buff = NULL;
		unsigned char ccif_sram[CCCI_EE_SIZE_CCIF_SRAM] = { 0 };

		if (buff)
			dest_buff = (unsigned int *)buff;
		else
			dest_buff = (unsigned int *)ccif_sram;
		if (length < sizeof(ccif_sram) && length > 0) {
			CCCI_ERROR_LOG(0, TAG,
				"dump CCIF SRAM length illegal %d/%zu\n",
				length, sizeof(ccif_sram));
			dest_buff = (unsigned int *)ccif_sram;
		} else {
			length = sizeof(ccif_sram);
		}

		ccci_hif_dump_status(1 << CCIF_HIF_ID, DUMP_FLAG_CCIF,
			dest_buff, length);
	}

	/*HIF related dump flag*/
	if (flag & (DUMP_FLAG_QUEUE_0_1 | DUMP_FLAG_QUEUE_0 |
		DUMP_FLAG_IRQ_STATUS | DUMP_FLAG_CLDMA))
		ccci_hif_dump_status(md->hif_flag, flag, NULL, length);

	if ((flag & DUMP_FLAG_REG) && md->hw_info->plat_ptr->debug_reg)
		md->hw_info->plat_ptr->debug_reg(md, true);
	if (flag & DUMP_FLAG_SMEM_EXP) {
		struct ccci_smem_region *mdccci_dbg =
			ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
		struct ccci_smem_region *mdss_dbg =
			ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);

		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump exception share memory\n");
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			mdccci_dbg->base_ap_view_vir, mdccci_dbg->size);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			mdss_dbg->base_ap_view_vir, mdss_dbg->size);
	}
	if (flag & DUMP_FLAG_SMEM_CCISM) {
		struct ccci_smem_region *scp =
			ccci_md_get_smem_by_user_id(SMEM_USER_CCISM_SCP);

		CCCI_MEM_LOG_TAG(0, TAG,
			"Dump CCISM share memory\n");
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			scp->base_ap_view_vir, scp->size);
	}
	if (flag & DUMP_FLAG_SMEM_CCB_CTRL) {
		struct ccci_smem_region *ccb_ctl =
			ccci_md_get_smem_by_user_id(SMEM_USER_RAW_CCB_CTRL);

		if (ccb_ctl) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"Dump CCB CTRL share memory\n");
			ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
				ccb_ctl->base_ap_view_vir,
				32 * ccb_configs_len * 2);
		}
	}
	if (flag & DUMP_FLAG_SMEM_CCB_DATA) {
		int i, j;
		unsigned char *curr_ch_p = NULL;
		unsigned int *curr_p = NULL;
		struct ccci_smem_region *ccb_data =
			ccci_md_get_smem_by_user_id(SMEM_USER_CCB_START);
		struct ccci_smem_region *dhl_raw =
			ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DHL);

		if (ccb_data && ccb_data->size != 0) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"Dump CCB DATA share memory\n");
			curr_ch_p = ccb_data->base_ap_view_vir;
			curr_p = (unsigned int *)curr_ch_p;
			for (i = 0; i < ccb_configs_len; i++) {
				/* dump dl buffer */
				for (j = 0; j < ccb_configs[i].dl_buff_size /
					ccb_configs[i].dl_page_size;  j++) {
					ccci_dump_write(
						CCCI_DUMP_MEM_DUMP, 0,
						"ul_buf%2d-page%2d %p: %08X %08X %08X %08X\n",
						i, j, curr_p,
						*curr_p, *(curr_p + 1),
						*(curr_p + 2), *(curr_p + 3));

					curr_ch_p +=
						ccb_configs[i].dl_page_size;
					curr_p = (unsigned int *)curr_ch_p;
				}

				/* dump ul buffer */
				for (j = 0; j < ccb_configs[i].ul_buff_size /
					ccb_configs[i].ul_page_size; j++) {
					ccci_dump_write(
						CCCI_DUMP_MEM_DUMP, 0,
						"dl_buf%2d-page%2d %p: %08X %08X %08X %08X\n",
						i, j, curr_p,
						*curr_p, *(curr_p + 1),
						*(curr_p + 2), *(curr_p + 3));

					curr_ch_p +=
						ccb_configs[i].ul_page_size;
					curr_p = (unsigned int *)curr_ch_p;
				}
			}
		}
		if (dhl_raw && dhl_raw->size) {
			CCCI_MEM_LOG_TAG(0, TAG,
				"Dump DHL RAW share memory\n");
			curr_ch_p = dhl_raw->base_ap_view_vir;
			curr_p = (unsigned int *)curr_ch_p;
			ccci_dump_write(
				CCCI_DUMP_MEM_DUMP, 0,
					"%p: %08X %08X %08X %08X\n",
					curr_p, *curr_p, *(curr_p + 1),
					*(curr_p + 2), *(curr_p + 3));
		}
	}

	if (flag & DUMP_FLAG_LAYOUT) {
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD layout struct\n");
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			&md->mem_layout, sizeof(struct ccci_mem_layout));
	}
	if (flag & DUMP_FLAG_MD_WDT) {
		CCCI_MEM_LOG_TAG(0, TAG,
			"wdt_enabled=%d\n", atomic_read(&md->wdt_enabled));
#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
		CCCI_NORMAL_LOG(0, TAG, "Dump md WDT IRQ status\n");
		mt_irq_dump_status(md->md_wdt_irq_id);
#endif
		CCCI_MEM_LOG_TAG(0, TAG, "Dump MD RGU registers\n");
		CCCI_MEM_LOG_TAG(0, TAG, "wdt_enabled=%d\n",
			atomic_read(&md->wdt_enabled));
	}

	if ((flag & DUMP_MD_BOOTUP_STATUS) &&
		md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status((unsigned int *)buff,
			length);

	return length;
}

static int md_cd_ee_callback(struct ccci_modem *md, enum MODEM_EE_FLAG flag)
{
	if (flag & EE_FLAG_ENABLE_WDT)
		wdt_enable_irq(md);
	if (flag & EE_FLAG_DISABLE_WDT)
		wdt_disable_irq(md);
	return 0;
}

static int md_cd_send_ccb_tx_notify(struct ccci_modem *md, int core_id)
{
	/* CCCI_NORMAL_LOG(0, TAG,
	 * "ccb tx notify to core %d\n", core_id);
	 */
	switch (core_id) {
	case P_CORE:
		ccci_hif_send_data(CCIF_HIF_ID, AP_MD_CCB_WAKEUP);
		break;
	case VOLTE_CORE:
	default:
		break;
	}
	return 0;
}

static struct ccci_modem_ops md_cd_ops = {
	.init = &md_cd_init,
	.start = &md_cd_start,
	.stop = &md_cd_stop,
	.soft_start = &md_cd_soft_start,
	.soft_stop = &md_cd_soft_stop,
	.pre_stop = &md_cd_pre_stop,
	.send_runtime_data = &md_cd_send_runtime_data_v2,
	.ee_handshake = &md_cd_ee_handshake,
	.force_assert = &md_cd_force_assert,
	.dump_info = &md_cd_dump_info,
	.ee_callback = &md_cd_ee_callback,
	.send_ccb_tx_notify = &md_cd_send_ccb_tx_notify,
};
int ccci_md_pre_stop(unsigned int stop_type)
{
	struct ccci_modem *md = ccci_get_modem();

	return md->ops->pre_stop(md, stop_type);
}

int ccci_md_stop(unsigned int stop_type)
{
	struct ccci_modem *md = ccci_get_modem();

	return md->ops->stop(md, stop_type);
}

int __weak md_cd_vcore_config(unsigned int hold_req)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

int ccci_md_pre_start(void)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md->hw_info->plat_ptr->vcore_config)
		return md->hw_info->plat_ptr->vcore_config(1);
	return -1;
}
int ccci_md_start(void)
{
	struct ccci_modem *md = ccci_get_modem();

	return md->ops->start(md);
}
int ccci_md_post_start(void)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md->hw_info->plat_ptr->vcore_config)
		return md->hw_info->plat_ptr->vcore_config(0);
	return -1;
}
int ccci_md_soft_stop(unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md->ops->soft_stop)
		return md->ops->soft_stop(md, sim_mode);
	return -1;
}
int ccci_md_soft_start(unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md->ops->soft_start)
		return md->ops->soft_start(md, sim_mode);
	return -1;
}

int ccci_md_send_runtime_data(void)
{
	struct ccci_modem *md = ccci_get_modem();

	return md->ops->send_runtime_data(md, CCCI_CONTROL_TX, 0, 0);
}

void ccci_md_dump_info(enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct ccci_modem *md = ccci_get_modem();

	if (md)
		md->ops->dump_info(md, flag, buff, length);
	else
		CCCI_ERROR_LOG(0, TAG, "invalid md!!\n");
}
//EXPORT_SYMBOL(ccci_md_dump_info);

void ccci_md_exception_handshake(int timeout)
{
	struct ccci_modem *md = ccci_get_modem();

	md->ops->ee_handshake(md, timeout);
}

int ccci_md_send_ccb_tx_notify(int core_id)
{
	struct ccci_modem *md = ccci_get_modem();

	return md->ops->send_ccb_tx_notify(md, core_id);
}

int ccci_md_force_assert(enum MD_FORCE_ASSERT_TYPE type,
	char *param, int len)
{
	int ret = 0;
	struct ccci_modem *md = ccci_get_modem();
	struct ccci_force_assert_shm_fmt *ccci_fa_smem_ptr = NULL;
	struct ccci_smem_region *force_assert =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_FORCE_ASSERT);

	if (md->is_force_asserted != 0)
		return ret;
	mdee_set_ex_time_str(type, param);
	if (type == MD_FORCE_ASSERT_BY_AP_MPU) {
		ret = md->ops->force_assert(md, CCIF_MPU_INTR);
	} else {
		ccci_fa_smem_ptr = (struct ccci_force_assert_shm_fmt *)
			force_assert->base_ap_view_vir;
		if (ccci_fa_smem_ptr) {
			ccci_fa_smem_ptr->error_code = type;
			if (param != NULL && len > 0) {
				if (len > force_assert->size -
				sizeof(struct ccci_force_assert_shm_fmt))
					len = force_assert->size -
					sizeof(
					struct ccci_force_assert_shm_fmt);
				memcpy_toio(ccci_fa_smem_ptr->param,
					param, len);
			}
		}
		ret = md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	md->is_force_asserted = 1;
	return ret;
}
EXPORT_SYMBOL(ccci_md_force_assert);

static ssize_t md_cd_debug_show(struct ccci_modem *md, char *buf)
{
	int curr = 0;

	curr = snprintf(buf, 16, "%d\n", ccci_debug_enable);
	if (curr < 0 || curr >= 16) {
		CCCI_ERROR_LOG(0, TAG,
			"%s-%d:snprintf fail,curr = %d\n", __func__, __LINE__, curr);
		return -1;
	}
	return curr;
}

static ssize_t md_cd_debug_store(struct ccci_modem *md,
	const char *buf, size_t count)
{
	ccci_debug_enable = buf[0] - '0';

	return count;
}

static ssize_t md_cd_dump_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count = scnprintf(buf, 256,
		"support: ccif cldma register smem image layout\n");
	return count;
}

static ssize_t md_cd_dump_store(struct ccci_modem *md,
	const char *buf, size_t count)
{
	enum MD_STATE md_state = ccci_fsm_get_md_state();
	/* echo will bring "xxx\n" here,
	 * so we eliminate the "\n" during comparing
	 */
	if (md_state != GATED && md_state != INVALID) {
		if (strncmp(buf, "ccif", count - 1) == 0)
			ccci_hif_dump_status(1 << CCIF_HIF_ID,
				DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF, NULL, 0);
		if ((strncmp(buf, "cldma", count - 1) == 0) &&
			(md->hif_flag&(1<<CLDMA_HIF_ID)))
			ccci_hif_dump_status(1 << CLDMA_HIF_ID,
				DUMP_FLAG_CLDMA, NULL, -1);
		if (strncmp(buf, "register", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
		if (strncmp(buf, "smem_exp", count-1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_SMEM_EXP, NULL, 0);
		if (strncmp(buf, "smem_ccism", count-1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_SMEM_CCISM, NULL, 0);
		if (strncmp(buf, "smem_ccb_ctrl", count-1) == 0)
			md->ops->dump_info(md,
				DUMP_FLAG_SMEM_CCB_CTRL, NULL, 0);
		if (strncmp(buf, "smem_ccb_data", count-1) == 0)
			md->ops->dump_info(md,
				DUMP_FLAG_SMEM_CCB_DATA, NULL, 0);
		if (strncmp(buf, "pccif", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_PCCIF_REG, NULL, 0);
		if (strncmp(buf, "layout", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_LAYOUT, NULL, 0);
		if (strncmp(buf, "mdslp", count - 1) == 0) {
			struct ccci_smem_region *low_pwr =
				ccci_md_get_smem_by_user_id(SMEM_USER_RAW_DBM);

			CCCI_MEM_LOG_TAG(0, TAG, "Dump MD SLP registers\n");
			ccci_util_cmpt_mem_dump(CCCI_DUMP_MEM_DUMP,
				low_pwr->base_ap_view_vir, low_pwr->size);
		}
		//md->ops->dump_info(md, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		if (strncmp(buf, "dpmaif", count - 1) == 0)
			ccci_hif_dump_status(1<<DPMAIF_HIF_ID,
				DUMP_FLAG_REG, NULL, -1);
		if (strncmp(buf, "port", count - 1) == 0)
			ccci_port_dump_status();
	}
	return count;
}

static ssize_t md_net_speed_show(struct ccci_modem *md, char *buf)
{
	return snprintf(buf, 4096, "curr netspeed log: %d\n",
		ccci_hif_dump_status(DPMAIF_HIF_ID, DUMP_FLAG_TOGGLE_NET_SPD,
			NULL, -1));
}

static ssize_t md_cd_parameter_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count += snprintf(buf + count, 128,
		"PACKET_HISTORY_DEPTH=%d\n", PACKET_HISTORY_DEPTH);
	count += snprintf(buf + count, 128, "BD_NUM=%ld\n", MAX_BD_NUM);

	return count;
}

static ssize_t md_cd_parameter_store(struct ccci_modem *md,
	const char *buf, size_t count)
{
	return count;
}
CCCI_MD_ATTR(NULL, debug, 0660, md_cd_debug_show, md_cd_debug_store);
CCCI_MD_ATTR(NULL, dump, 0660, md_cd_dump_show, md_cd_dump_store);
CCCI_MD_ATTR(NULL, net_speed, 0660, md_net_speed_show, NULL);
CCCI_MD_ATTR(NULL, parameter, 0660, md_cd_parameter_show,
	md_cd_parameter_store);

static void md_cd_sysfs_init(struct ccci_modem *md)
{
	int ret;

	ccci_md_attr_debug.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_debug.attr);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_debug.attr.name, ret);

	ccci_md_attr_dump.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_dump.attr);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_dump.attr.name, ret);

	ccci_md_attr_net_speed.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_net_speed.attr);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_net_speed.attr.name, ret);

	ccci_md_attr_parameter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_parameter.attr);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_parameter.attr.name, ret);
}

void ccci_hif_cldma_restore_reg(struct ccci_modem *md)
{
}

static void ccci_modem_restore_reg(struct ccci_modem *md)
{
	enum MD_STATE md_state = ccci_fsm_get_md_state();

	if (md_state == GATED || md_state == WAITING_TO_STOP ||
		md_state == INVALID) {
		CCCI_NORMAL_LOG(0, TAG,
			"Resume no need restore for md_state=%d\n", md_state);
		return;
	}

	if (md->hif_flag & (1 << CLDMA_HIF_ID))
		ccci_hif_cldma_restore_reg(md);

	ccci_hif_resume(md->hif_flag);
}

int ccci_modem_syssuspend(void)
{
	struct ccci_modem *md;

	CCCI_DEBUG_LOG(0, TAG, "%s only for MD\n", __func__);
	md = ccci_get_modem();
	if (md != NULL)
		ccci_hif_suspend(md->hif_flag);
	return 0;
}

void ccci_modem_sysresume(void)
{
	struct ccci_modem *md;

	CCCI_DEBUG_LOG(0, TAG, "%s should be no used\n", __func__);
	md = ccci_get_modem();
	if (md != NULL)
		ccci_modem_restore_reg(md);
}


static struct syscore_ops ccci_modem_sysops = {
	.suspend = ccci_modem_syssuspend,
	.resume = ccci_modem_sysresume,
};

/* setup function is only for data structure initialization */
struct ccci_modem *ccci_md_alloc(int private_size)
{
	struct ccci_modem *md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);

	if (!md) {
		CCCI_ERROR_LOG(-1, TAG,
			"fail to allocate memory for modem structure\n");
		goto out;
	}
	if (private_size > 0)
		md->private_data = kzalloc(private_size, GFP_KERNEL);
	else
		md->private_data = NULL;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
	md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_AP_REG;

 out:
	return md;
}

void ccci_md_config(struct ccci_modem *md)
{
	/* setup config */
	md->per_md_data.config.load_type = get_md_img_type();
	if (get_modem_is_enabled())
		md->per_md_data.config.setting |= MD_SETTING_ENABLE;
	else
		md->per_md_data.config.setting &= ~MD_SETTING_ENABLE;


	ap_md_mem_init(&md->mem_layout);

	/* updae image info */
	md->per_md_data.img_info[IMG_MD].type = IMG_MD;
	md->per_md_data.img_info[IMG_MD].address =
		md->mem_layout.md_bank0.base_ap_view_phy;
	md->per_md_data.img_info[IMG_DSP].type = IMG_DSP;
	md->per_md_data.img_info[IMG_ARMV7].type = IMG_ARMV7;
}

int boot_md_show(char *buf, int size)
{
	int curr = 0;

	if (get_modem_is_enabled())
		curr += snprintf(&buf[curr], size, "md1:%d",
			ccci_fsm_get_md_state());
	return curr;
}

int boot_md_store(void)
{
	return -EACCES;
}

static void ccci_md_obj_release(struct kobject *kobj)
{
	CCCI_ERROR_LOG(-1, CORE, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->store)
		len = a->store(a->modem, buf, count);

	return len;
}

static const struct sysfs_ops ccci_md_sysfs_ops = {
	.show = ccci_md_attr_show,
	.store = ccci_md_attr_store
};

static struct attribute *ccci_md_default_attrs[] = {
	NULL
};

static struct kobj_type ccci_md_ktype = {
	.release = ccci_md_obj_release,
	.sysfs_ops = &ccci_md_sysfs_ops,
	.default_attrs = ccci_md_default_attrs
};

void ccci_sysfs_add_md(void *kobj)
{
	ccci_sysfs_add_modem((void *)kobj, (void *)&ccci_md_ktype,
		boot_md_show, boot_md_store);
}

int ccci_md_register(struct ccci_modem *md)
{
	int ret;

	/* init per-modem sub-system */
	CCCI_INIT_LOG(0, TAG, "register modem\n");
	/* init modem */
	ret = md->ops->init(md);
	if (ret < 0)
		return ret;
	ccci_md_config(md);

	modem_sys = md;
	ccci_sysfs_add_md((void *)&md->kobj);
	ccci_fsm_init();
	ccci_port_init();
	return 0;
}

static u64 cldma_dmamask = DMA_BIT_MASK(36);

int ccci_modem_init_common(struct platform_device *plat_dev,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *md_hw)
{
	struct ccci_modem *md = NULL;
	struct md_sys1_info *md_info = NULL;
	int ret;

	/* Allocate md ctrl memory and do initialize */
	md = ccci_md_alloc(sizeof(struct md_sys1_info));
	if (md == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc modem ctrl mem fail\n", __func__);
		return -1;
	}

	md->per_md_data.md_capability = dev_cfg->capability;
	md->hw_info = md_hw;

	md->plat_dev = plat_dev;
	md->plat_dev->dev.dma_mask = &cldma_dmamask;
	md->plat_dev->dev.coherent_dma_mask = cldma_dmamask;
	md->ops = &md_cd_ops;
	CCCI_INIT_LOG(0, TAG,
		"%s:md=%p,md->private_data=%p\n", __func__,
		md, md->private_data);

	/* register modem */
	ccci_md_register(md);

	/* init modem private data */
	md_info = (struct md_sys1_info *)md->private_data;

	scnprintf(md->trm_wakelock_name, sizeof(md->trm_wakelock_name),
		"md_cldma_trm");
	md->trm_wake_lock = wakeup_source_register(NULL, md->trm_wakelock_name);
	if (!md->trm_wake_lock) {
		CCCI_ERROR_LOG(0, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	scnprintf(md_info->peer_wakelock_name,
		sizeof(md_info->peer_wakelock_name), "md_cldma_peer");

	md_info->peer_wake_lock =
		wakeup_source_register(NULL, md_info->peer_wakelock_name);
	if (!md_info->peer_wake_lock) {
		CCCI_ERROR_LOG(0, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	/* Copy HW info */
	md_info->ap_ccif_irq_id = md_hw->ap_ccif_irq1_id;
	md_info->channel_id = 0;
	atomic_set(&md_info->ccif_irq_enabled, 1);

	md->md_wdt_irq_id = md_hw->md_wdt_irq_id;
	atomic_set(&md->reset_on_going, 1);
	/* IRQ is default enabled after request_irq */
	atomic_set(&md->wdt_enabled, 1);

	ret = of_property_read_u32(plat_dev->dev.of_node,
		"mediatek,mdhif-type", &md->hif_flag);
	if (ret != 0)
		md->hif_flag = (1 << MD1_NET_HIF | 1 << MD1_NORMAL_HIF);

	/* register SYS CORE suspend resume call back */
	register_syscore_ops(&ccci_modem_sysops);

	/* add sysfs entries */
	md_cd_sysfs_init(md);
	/* hook up to device */
	plat_dev->dev.platform_data = md;

	return 0;
}
EXPORT_SYMBOL(ccci_modem_init_common);

int Is_MD_EMI_voilation(void)
{
	return 1;
}

