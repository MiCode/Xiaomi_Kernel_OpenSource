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

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#ifdef MTK_CCCI_TODO_PART /* TODO */
#include <mach/mtk_pbm.h>
#endif
#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "md_sys1_platform.h"
#include "modem_reg_base.h"
#include "ccci_debug.h"
#include "hif/ccci_hif_cldma.h"
#if (MD_GENERATION >= 6293)
#include "hif/ccci_hif_ccif.h"
#endif

#define TAG "mcd"

void ccif_enable_irq(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (atomic_cmpxchg(&md_info->ccif_irq_enabled, 0, 1) == 0) {
		enable_irq(md_info->ap_ccif_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "enable ccif irq\n");
	}
}

void ccif_disable_irq(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	if (atomic_cmpxchg(&md_info->ccif_irq_enabled, 1, 0) == 1) {
		disable_irq_nosync(md_info->ap_ccif_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "disable ccif irq\n");
	}
}

void wdt_enable_irq(struct ccci_modem *md)
{
	if (atomic_cmpxchg(&md->wdt_enabled, 0, 1) == 0) {
		enable_irq(md->md_wdt_irq_id);
		CCCI_NORMAL_LOG(md->index, TAG, "enable wdt irq\n");
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
		CCCI_NORMAL_LOG(md->index, TAG, "disable wdt irq\n");
	}
}

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;

	CCCI_ERROR_LOG(md->index, TAG, "MD WDT IRQ\n");

	ccif_disable_irq(md);
	wdt_disable_irq(md);

	ccci_event_log("md%d: MD WDT IRQ\n", md->index);
#ifndef DISABLE_MD_WDT_PROCESS
	/* 1. disable MD WDT */
#ifdef ENABLE_MD_WDT_DBG
	unsigned int state;

	state = ccci_read32(md->md_rgu_base, WDT_MD_STA);
	ccci_write32(md->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG,
		"WDT IRQ disabled for debug, state=%X\n", state);
#ifdef L1_BASE_ADDR_L1RGU
	state = ccci_read32(md->l1_rgu_base, REG_L1RSTCTL_WDT_STA);
	ccci_write32(md->l1_rgu_base,
		REG_L1RSTCTL_WDT_MODE, L1_WDT_MD_MODE_KEY);
	CCCI_NORMAL_LOG(md->index, TAG,
		"WDT IRQ disabled for debug, L1 state=%X\n", state);
#endif
#endif
#endif
	ccci_fsm_recv_md_interrupt(md->index, MD_IRQ_WDT);
	return IRQ_HANDLED;
}

static int md_cd_ccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;

	busy = ccif_read32(md_info->ap_ccif_base, APCCIF_BUSY);
	if (busy & (1 << channel_id))
		return -1;
	ccif_write32(md_info->ap_ccif_base, APCCIF_BUSY,
		1 << channel_id);
	ccif_write32(md_info->ap_ccif_base, APCCIF_TCHNUM,
		channel_id);
	return 0;
}

static void md_cd_ccif_delayed_work(struct ccci_modem *md)
{
	if (md->hif_flag & (1<<CLDMA_HIF_ID))  {
		/* stop CLDMA, we don't want to get CLDMA IRQ when MD is
		 * resetting CLDMA after it got cleaq_ack
		 */
		cldma_stop(CLDMA_HIF_ID);
		CCCI_NORMAL_LOG(md->index, TAG,
			"%s: stop cldma done\n", __func__);
		/*dump rxq after cldma stop to avoid race condition*/
		ccci_hif_dump_status(1 << CLDMA_HIF_ID, DUMP_FLAG_QUEUE_0_1,
			1 << IN);
		CCCI_NORMAL_LOG(md->index, TAG,
			"%s: dump queue0-1 done\n", __func__);
		md_cldma_hw_reset(md->index);
		CCCI_NORMAL_LOG(md->index, TAG,
			"%s: hw reset done\n", __func__);
		md_cd_clear_all_queue(CLDMA_HIF_ID, IN);
	}
#if (MD_GENERATION >= 6293)
	md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
	md_ccif_reset_queue(CCIF_HIF_ID, 0);
#endif
	/* tell MD to reset CLDMA */
	md_cd_ccif_send(md, H2D_EXCEPTION_CLEARQ_ACK);
	CCCI_NORMAL_LOG(md->index, TAG, "send clearq_ack to MD\n");
}

static void md_cd_exception(struct ccci_modem *md, enum HIF_EX_STAGE stage)
{
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_MDCCCI_DBG);

	CCCI_ERROR_LOG(md->index, TAG, "MD exception HIF %d\n", stage);
	ccci_event_log("md%d:MD exception HIF %d\n", md->index, stage);
	/* in exception mode, MD won't sleep, so we do not
	 * need to request MD resource first
	 */
	switch (stage) {
	case HIF_EX_INIT:
#if (MD_GENERATION >= 6293)
		ccci_hif_dump_status(1 << CCIF_HIF_ID, DUMP_FLAG_CCIF |
			DUMP_FLAG_IRQ_STATUS, 0);
#endif
		if (*((int *)(mdccci_dbg->base_ap_view_vir +
			CCCI_SMEM_OFFSET_SEQERR)) != 0) {
			CCCI_ERROR_LOG(md->index, TAG,
				"MD found wrong sequence number\n");
		}
		if (md->hif_flag & (1<<CLDMA_HIF_ID))  {
			CCCI_ERROR_LOG(md->index, TAG,
				"dump cldma on ccif hs0\n");
			ccci_hif_dump_status(1 << CLDMA_HIF_ID,
				DUMP_FLAG_CLDMA, -1);
			/* disable CLDMA except un-stop queues */
			cldma_stop_for_ee(CLDMA_HIF_ID);
			/* purge Tx queue */
			md_cd_clear_all_queue(CLDMA_HIF_ID, OUT);
		}
		ccci_hif_md_exception(md->hif_flag, stage);
		/* Rx dispatch does NOT depend on queue index
		 * in port structure, so it still can find right port.
		 */
		md_cd_ccif_send(md, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		break;
	case HIF_EX_CLEARQ_DONE:
		/* give DHL some time to flush data */
		msleep(2000);
		ccci_hif_md_exception(md->hif_flag, stage);
		md_cd_ccif_delayed_work(md);
		break;
	case HIF_EX_ALLQ_RESET:
		md->per_md_data.is_in_ee_dump = 1;
		if (md->hif_flag & (1<<CLDMA_HIF_ID))
			md_cd_ccif_allQreset_work(CLDMA_HIF_ID);
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
			CCCI_DEBUG_LOG(md->index, TAG,
				"poll RCHNUM %d\n", md_info->channel_id);
			return;
		}
		msleep(time_once);
		cnt--;
	}
	CCCI_ERROR_LOG(md->index, TAG,
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

static irqreturn_t md_cd_ccif_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;

	/* must ack first, otherwise IRQ will rush in */
	md_info->channel_id = ccif_read32(md_info->ap_ccif_base,
		APCCIF_RCHNUM);
	CCCI_DEBUG_LOG(md->index, TAG,
		"MD CCIF IRQ 0x%X\n", md_info->channel_id);
	/*don't ack data queue to avoid missing rx intr*/
	ccif_write32(md_info->ap_ccif_base, APCCIF_ACK,
		md_info->channel_id & (0xFFFF << RINGQ_EXP_BASE));

#if (MD_GENERATION <= 6292)
	if (md_info->channel_id & (1 << AP_MD_CCB_WAKEUP)) {
		struct ccci_smem_region *ccb_ctl =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_CCB_CTRL);
		unsigned int *debug_addr =
			(unsigned int *)ccb_ctl->base_ap_view_vir;

		CCCI_DEBUG_LOG(md->index, TAG, "CCB wakeup\n");
		CCCI_DEBUG_LOG(md->index, TAG,
			"GS=%X, a=%X, f=%X, r=%X, w=%X, page_size=%X, buf_size=%X, GE=%X\n",
			debug_addr[0], debug_addr[1], debug_addr[2],
			debug_addr[3], debug_addr[4], debug_addr[5],
			debug_addr[6], debug_addr[7]);

		ccci_port_queue_status_notify(md->index, CCIF_HIF_ID,
			AP_MD_CCB_WAKEUP, -1, RX_IRQ);
	}
#endif
	if (md_info->channel_id & (1<<AP_MD_PEER_WAKEUP))
		__pm_wakeup_event(md_info->peer_wake_lock,
			jiffies_to_msecs(HZ));
	if (md_info->channel_id & (1<<AP_MD_SEQ_ERROR)) {
		CCCI_ERROR_LOG(md->index, TAG, "MD check seq fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
	}

	if (md_info->channel_id & (1 << D2H_EXCEPTION_INIT)) {
		/* do not disable IRQ, as CCB still needs it */
		ccci_fsm_recv_md_interrupt(md->index, MD_IRQ_CCIF_EX);
	}

	return IRQ_HANDLED;
}

static inline int md_sys1_sw_init(struct ccci_modem *md)
{
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;
	int ret;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"%s, MD_WDT IRQ(%d) CCIF_IRQ %d\n", __func__,
		md->md_wdt_irq_id, md_info->ap_ccif_irq_id);
	ret = request_irq(md->md_wdt_irq_id, md_cd_wdt_isr,
			md->md_wdt_irq_flags, "MD_WDT", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"request MD_WDT IRQ(%d) error %d\n",
			md->md_wdt_irq_id, ret);
		return ret;
	}
	/* IRQ is enabled after requested, so call enable_irq after
	 * request_irq will get a unbalance warning
	 */
	ret = request_irq(md_info->ap_ccif_irq_id, md_cd_ccif_isr,
			md_info->ap_ccif_irq_flags, "CCIF0_AP", md);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"request CCIF0_AP IRQ(%d) error %d\n",
			md_info->ap_ccif_irq_id, ret);
		return ret;
	}
	return 0;
}

static int md_cd_init(struct ccci_modem *md)
{
	CCCI_INIT_LOG(md->index, TAG, "CCCI: modem is initializing\n");

	return 0;
}

/* Please delete this function once it can be deleted. */
static int ccci_md_hif_start(struct ccci_modem *md, int stage)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	switch (stage) {
	case 1:
		/*enable clk: cldma & ccif */
		ccci_set_clk_cg(md, 1);
		if (md->hif_flag & (1 << CCIF_HIF_ID)) {
#if (MD_GENERATION >= 6293)
			md_ccif_sram_reset(CCIF_HIF_ID);
			md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
			md_ccif_reset_queue(CCIF_HIF_ID, 1);
			md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_NORMAL);
			md_ccif_reset_queue(CCIF_HIF_ID, 1);
#endif

			/* 3. enable MPU */

			/* clear all ccif irq before enable it.*/
			ccci_reset_ccif_hw(md->index, AP_MD1_CCIF,
				md_info->ap_ccif_base, md_info->md_ccif_base);
		}
		if (md->hif_flag & (1 << CLDMA_HIF_ID)) {
			/* 2. clearring buffer, just in case */
			md_cd_clear_all_queue(CLDMA_HIF_ID, OUT);
			md_cd_clear_all_queue(CLDMA_HIF_ID, IN);
			md_cldma_hw_reset(md->index);
		}
		break;
	case 2:
		if (md->hif_flag & (1 << CCIF_HIF_ID))
			ccif_enable_irq(md);
		if (md->hif_flag & (1 << CLDMA_HIF_ID)) {
			/* 8. start CLDMA */
			cldma_reset(CLDMA_HIF_ID);
			cldma_start(CLDMA_HIF_ID);
		}
		break;
	default:
		break;
	}
	return 0;
}

int __weak md_start_platform(struct ccci_modem *md)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

static int md_cd_start(struct ccci_modem *md)
{
	int ret = 0;

	if (md->per_md_data.config.setting & MD_SETTING_FIRST_BOOT) {
		md_cd_io_remap_md_side_register(md);
		md_sys1_sw_init(md);

		ccci_hif_late_init(md->index, md->hif_flag);
		/* init security, as security depends on dummy_char,
		 * which is ready very late.
		 */
		ccci_init_security();
		ccci_md_clear_smem(md->index, 1);
#if (MD_GENERATION >= 6293)
		md_ccif_ring_buf_init(CCIF_HIF_ID);
#endif
		ret = md_start_platform(md);
		if (ret) {
			CCCI_BOOTUP_LOG(md->index, TAG,
				"power on MD BROM fail %d\n", ret);
			goto out;
		}
		md->per_md_data.config.setting &= ~MD_SETTING_FIRST_BOOT;
	} else
		ccci_md_clear_smem(md->index, 0);

	CCCI_BOOTUP_LOG(md->index, TAG, "modem is starting\n");
	/* 1. load modem image */

	CCCI_BOOTUP_LOG(md->index, TAG,
		"modem image ready, bypass load\n");
	ret = ccci_get_md_check_hdr_inf(md->index,
		&md->per_md_data.img_info[IMG_MD],
		md->per_md_data.img_post_fix);
	if (ret < 0) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"partition read fail(%d)\n", ret);
		/* goto out; */
	} else
		CCCI_BOOTUP_LOG(md->index, TAG,
			"partition read success\n");

#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 1, MD_FLIGHT_MODE_NONE);
#endif

	ccci_md_hif_start(md, 1);
	/* 4. power on modem, do NOT touch MD register before this */
	ret = md_cd_power_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"power on MD fail %d\n", ret);
		goto out;
	}
#ifdef SET_EMI_STEP_BY_STAGE
	ccci_set_mem_access_protection_1st_stage(md);
#endif
	/* 5. update mutex */
	atomic_set(&md->reset_on_going, 0);

	md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_AP_REG;

	/* 7. let modem go */
	md_cd_let_md_go(md);
	wdt_enable_irq(md);
	ccci_md_hif_start(md, 2);

	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
 out:
	CCCI_BOOTUP_LOG(md->index, TAG,
		"modem started %d\n", ret);
	/* used for throttling feature - start */
	/*ccci_modem_boot_count[md->index]++;*/
	/* used for throttling feature - end */
	return ret;
}

static int check_power_off_en(struct ccci_modem *md)
{
	int smem_val;
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_MDSS_DBG);

	if (md->index != MD_SYS1)
		return 1;

	smem_val = *((int *)((long)mdss_dbg->base_ap_view_vir +
		CCCI_EE_OFFSET_EPOF_MD1));
	CCCI_NORMAL_LOG(md->index, TAG,
		"share for power off:%x\n", smem_val);
	if (smem_val != 0) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"[ccci]enable power off check\n");
		return 1;
	}
	CCCI_NORMAL_LOG(md->index, TAG,
		"disable power off check\n");
	return 0;
}

static int md_cd_soft_start(struct ccci_modem *md, unsigned int mode)
{
	return md_cd_soft_power_on(md, mode);
}

static int md_cd_soft_stop(struct ccci_modem *md, unsigned int mode)
{
	return md_cd_soft_power_off(md, mode);
}

void __weak md1_sleep_timeout_proc(void)
{
	CCCI_DEBUG_LOG(-1, TAG, "No %s\n", __func__);
}

static int md_cd_pre_stop(struct ccci_modem *md, unsigned int stop_type)
{
	int count = 0;
	int en_power_check;
	u32 pending;
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_MDSS_DBG);
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data(md->index);
	int md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;

	/* 1. mutex check */
	if (atomic_add_return(1, &md->reset_on_going) > 1) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}

	CCCI_NORMAL_LOG(md->index, TAG,
		"%s: CCCI modem is resetting\n", __func__);
	/* 2. disable WDT IRQ */
	wdt_disable_irq(md);

	en_power_check = check_power_off_en(md);

	/* only debug in Flight mode */
	if (stop_type == MD_FLIGHT_MODE_ENTER) {
		count = 5;
		while (spm_is_md1_sleep() == 0) {
			count--;
			if (count == 0) {
				if (en_power_check) {
					CCCI_NORMAL_LOG(md->index, TAG,
					"MD is not in sleep mode, dump md status!\n");
					CCCI_MEM_LOG_TAG(md->index, TAG,
					"Dump MD EX log\n");
					if (md_dbg_dump_flag &
						(1 << MD_DBG_DUMP_SMEM)) {
						ccci_util_mem_dump(md->index,
						CCCI_DUMP_MEM_DUMP,
						mdccci_dbg->base_ap_view_vir,
						mdccci_dbg->size);
						ccci_util_mem_dump(md->index,
						CCCI_DUMP_MEM_DUMP,
						mdss_dbg->base_ap_view_vir,
						mdss_dbg->size);
					}
					md_cd_dump_debug_register(md);
					/* cldma_dump_register(CLDMA_HIF_ID);*/
#if defined(CONFIG_MTK_AEE_FEATURE)
					aed_md_exception_api(
					mdss_dbg->base_ap_view_vir,
					mdss_dbg->size, NULL, 0,
					"After AP send EPOF, MD didn't go to sleep in 4 seconds.",
					DB_OPT_DEFAULT);
#endif
				} else
					md1_sleep_timeout_proc();
				break;
			}
			md_cd_lock_cldma_clock_src(1);
			msleep(1000);
			md_cd_lock_cldma_clock_src(0);
			msleep(20);
		}
		pending = mt_irq_get_pending(md->md_wdt_irq_id);
		if (pending) {
			CCCI_NORMAL_LOG(md->index, TAG, "WDT IRQ occur.");
			CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD EX log\n");
			if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
				ccci_util_mem_dump(md->index,
					CCCI_DUMP_MEM_DUMP,
					mdccci_dbg->base_ap_view_vir,
					mdccci_dbg->size);
				ccci_util_mem_dump(md->index,
					CCCI_DUMP_MEM_DUMP,
					mdss_dbg->base_ap_view_vir,
					mdss_dbg->size);
			}

			md_cd_dump_debug_register(md);
			/* cldma_dump_register(CLDMA_HIF_ID);*/
#if defined(CONFIG_MTK_AEE_FEATURE)
			aed_md_exception_api(NULL, 0, NULL, 0,
				"WDT IRQ occur.", DB_OPT_DEFAULT);
#endif
		}
	}

	CCCI_NORMAL_LOG(md->index, TAG, "Reset when MD state: %d\n",
			ccci_fsm_get_md_state(md->index));
	return 0;
}

static int md_cd_stop(struct ccci_modem *md, unsigned int stop_type)
{
	int ret = 0;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

	CCCI_NORMAL_LOG(md->index, TAG,
		"modem is power off, stop_type=%d\n", stop_type);
	ccif_disable_irq(md);

	md_cd_check_emi_state(md, 1);	/* Check EMI before */

	/* power off MD */
	ret = md_cd_power_off(md,
		stop_type == MD_FLIGHT_MODE_ENTER ? 100 : 0);
	CCCI_NORMAL_LOG(md->index, TAG,
		"modem is power off done, %d\n", ret);
	if (md->hif_flag & (1<<CLDMA_HIF_ID))
		md_cldma_clear(CLDMA_HIF_ID);

	/* ACK CCIF for MD. while entering flight mode,
	 * we may send something after MD slept
	 */
	ccci_reset_ccif_hw(md->index, AP_MD1_CCIF,
		md_info->ap_ccif_base, md_info->md_ccif_base);
	md_cd_check_emi_state(md, 0);	/* Check EMI after */

	/*disable cldma & ccif clk*/
	ccci_set_clk_cg(md, 0);

#ifdef FEATURE_BSI_BPI_SRAM_CFG
	ccci_set_bsi_bpi_SRAM_cfg(md, 0, stop_type);
#endif

	return 0;
}

static void dump_runtime_data_v2(struct ccci_modem *md,
	struct ap_query_md_feature *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = BOOT_INFO; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"feature %u: mask %u, version %u\n",
			i, ap_feature->feature_set[i].support_mask,
			ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"share_memory_support 0x%x\n",
		ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_addr 0x%x\n",
		ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_size 0x%x\n",
		ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_addr 0x%x\n",
		ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_size 0x%x\n",
		ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_start_addr 0x%x\n",
		ap_feature->set_md_mpu_start_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_total_size 0x%x\n",
		ap_feature->set_md_mpu_total_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"tail_pattern 0x%x\n",
		ap_feature->tail_pattern);
}

static void dump_runtime_data_v2_1(struct ccci_modem *md,
	struct ap_query_md_feature_v2_1 *ap_feature)
{
	u8 i = 0;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"head_pattern 0x%x\n", ap_feature->head_pattern);

	for (i = BOOT_INFO; i < AP_RUNTIME_FEATURE_ID_MAX; i++) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"feature %u: mask %u, version %u\n",
			i, ap_feature->feature_set[i].support_mask,
			ap_feature->feature_set[i].version);
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"share_memory_support 0x%x\n",
		ap_feature->share_memory_support);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_addr 0x%x\n",
		ap_feature->ap_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"ap_runtime_data_size 0x%x\n",
		ap_feature->ap_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_addr 0x%x\n",
		ap_feature->md_runtime_data_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"md_runtime_data_size 0x%x\n",
		ap_feature->md_runtime_data_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_noncached_start_addr 0x%x\n",
		ap_feature->noncached_mpu_start_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_noncached_total_size 0x%x\n",
		ap_feature->noncached_mpu_total_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_cached_start_addr 0x%x\n",
		ap_feature->cached_mpu_start_addr);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"set_md_mpu_cached_total_size 0x%x\n",
		ap_feature->cached_mpu_total_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"tail_pattern 0x%x\n",
		ap_feature->tail_pattern);
}

static void md_cd_smem_sub_region_init(struct ccci_modem *md)
{
	int __iomem *addr;
	int i;
	struct ccci_smem_region *dbm =
		ccci_md_get_smem_by_user_id(md->index, SMEM_USER_RAW_DBM);

	/* Region 0, dbm */
	addr = (int __iomem *)dbm->base_ap_view_vir;
	addr[0] = 0x44444444; /* Guard pattern 1 header */
	addr[1] = 0x44444444; /* Guard pattern 2 header */
#ifdef DISABLE_PBM_FEATURE
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM/4+2); i++)
		addr[i] = 0xFFFFFFFF;
#else
	for (i = 2; i < (CCCI_SMEM_SIZE_DBM/4+2); i++)
		addr[i] = 0x00000000;
#endif
	addr[i++] = 0x44444444; /* Guard pattern 1 tail */
	addr[i++] = 0x44444444; /* Guard pattern 2 tail */

	/* Notify PBM */
#ifdef MTK_CCCI_TODO_PART /* TODO */
#ifndef DISABLE_PBM_FEATURE
	init_md_section_level(KR_MD1);
#endif
#endif
}

static void config_ap_runtime_data_v2(struct ccci_modem *md,
	struct ap_query_md_feature *ap_feature)
{
	struct ccci_smem_region *runtime_data =
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_RUNTIME_DATA);

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
		ccci_md_get_smem_by_user_id(md->index,
			SMEM_USER_RAW_RUNTIME_DATA);

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

#if (MD_GENERATION <= 6292)
static struct sk_buff *md_cd_init_rt_header(struct ccci_modem *md,
	int packet_size, unsigned int tx_ch, int skb_from_pool)
{
	struct ccci_header *ccci_h;
	struct sk_buff *skb = NULL;

	skb = ccci_alloc_skb(packet_size, skb_from_pool, 1);
	CCCI_BOOTUP_LOG(md->index, TAG,
		"runtime feature skb size: 0x%x\n", packet_size);

	if (!skb)
		return NULL;
	ccci_h = (struct ccci_header *)skb->data;

	/*header */
	ccci_h->data[0] = 0x00;
	ccci_h->data[1] = packet_size;
	ccci_h->reserved = MD_INIT_CHK_ID;
	ccci_h->channel = tx_ch;

	return skb;
}
#endif

static int md_cd_send_runtime_data_v2(struct ccci_modem *md,
	unsigned int tx_ch, unsigned int txqno, int skb_from_pool)
{
	int packet_size;
#if (MD_GENERATION <= 6292)
	struct sk_buff *skb = NULL;
#endif
	struct ap_query_md_feature *ap_rt_data;
	struct ap_query_md_feature_v2_1 *ap_rt_data_v2_1;
	int ret;

	if (md->runtime_version < AP_MD_HS_V2) {
		CCCI_ERROR_LOG(md->index, TAG,
			"unsupported runtime version %d\n",
			md->runtime_version);
		return -CCCI_ERR_CCIF_INVALID_RUNTIME_LEN;
	}

	if (md->multi_md_mpu_support) {
		packet_size = sizeof(struct ap_query_md_feature_v2_1) +
			sizeof(struct ccci_header);
#if (MD_GENERATION <= 6292)
		skb = md_cd_init_rt_header(md, packet_size, tx_ch,
				skb_from_pool);
		ap_rt_data_v2_1 =
			(struct ap_query_md_feature_v2_1 *)(skb->data +
			sizeof(struct ccci_header));
#else
		ap_rt_data_v2_1 = (struct ap_query_md_feature_v2_1 *)
			ccif_hif_fill_rt_header(CCIF_HIF_ID, packet_size,
			tx_ch, txqno);
#endif
		memset_io(ap_rt_data_v2_1, 0,
			sizeof(struct ap_query_md_feature_v2_1));
		config_ap_runtime_data_v2_1(md, ap_rt_data_v2_1);
		dump_runtime_data_v2_1(md, ap_rt_data_v2_1);
	} else {
		/* infactly, 6292 should not be this else condition */
		packet_size = sizeof(struct ap_query_md_feature) +
			sizeof(struct ccci_header);
#if (MD_GENERATION <= 6292)
		skb = md_cd_init_rt_header(md, packet_size, tx_ch,
			skb_from_pool);
		ap_rt_data = (struct ap_query_md_feature *)(skb->data +
			sizeof(struct ccci_header));
#else
		ap_rt_data = (struct ap_query_md_feature *)
			ccif_hif_fill_rt_header(CCIF_HIF_ID, packet_size,
			tx_ch, txqno);
#endif
		memset_io(ap_rt_data, 0, sizeof(struct ap_query_md_feature));
		config_ap_runtime_data_v2(md, ap_rt_data);
		dump_runtime_data_v2(md, ap_rt_data);
	}

	md_cd_smem_sub_region_init(md);

#if (MD_GENERATION <= 6292)
	skb_put(skb, packet_size);
	ret = ccci_hif_send_skb(CLDMA_HIF_ID, txqno, skb,
		skb_from_pool, 1);
#else
	ret = md_ccif_send(CCIF_HIF_ID, H2D_SRAM);
#endif
	return ret;
}

static int md_cd_force_assert(struct ccci_modem *md, enum MD_COMM_TYPE type)
{
	CCCI_NORMAL_LOG(md->index, TAG,
		"force assert MD using %d\n", type);
	if (type == CCIF_INTERRUPT)
		md_cd_ccif_send(md, AP_MD_SEQ_ERROR);
	else if (type == CCIF_MPU_INTR) {
		md_cd_ccif_send(md, H2D_MPU_FORCE_ASSERT);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG, NULL, 0);
	}
	return 0;
}

static void md_cd_dump_ccif_reg(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	int idx;

	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_CON(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_CON,
		ccif_read32(md_info->ap_ccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_BUSY(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_BUSY,
		ccif_read32(md_info->ap_ccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_START(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_START,
		ccif_read32(md_info->ap_ccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_TCHNUM(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_TCHNUM,
		ccif_read32(md_info->ap_ccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_RCHNUM(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_RCHNUM,
		ccif_read32(md_info->ap_ccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "AP_ACK(%p)=%x\n",
		md_info->ap_ccif_base + APCCIF_ACK,
		ccif_read32(md_info->ap_ccif_base, APCCIF_ACK));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_CON(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_CON,
		ccif_read32(md_info->md_ccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_BUSY(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_BUSY,
		ccif_read32(md_info->md_ccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_START(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_START,
		ccif_read32(md_info->md_ccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_TCHNUM(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_TCHNUM,
		ccif_read32(md_info->md_ccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_RCHNUM(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_RCHNUM,
		ccif_read32(md_info->md_ccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG, "MD_ACK(%p)=%x\n",
		md_info->md_ccif_base + APCCIF_ACK,
		ccif_read32(md_info->md_ccif_base, APCCIF_ACK));

	for (idx = 0; idx < md->hw_info->sram_size / sizeof(u32);
		idx += 4) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"CHDATA(%p): %08X %08X %08X %08X\n",
			md_info->ap_ccif_base + APCCIF_CHDATA +
			idx * sizeof(u32),
			ccif_read32(md_info->ap_ccif_base + APCCIF_CHDATA,
				(idx + 0) * sizeof(u32)),
			ccif_read32(md_info->ap_ccif_base + APCCIF_CHDATA,
				(idx + 1) * sizeof(u32)),
			ccif_read32(md_info->ap_ccif_base + APCCIF_CHDATA,
				(idx + 2) * sizeof(u32)),
			ccif_read32(md_info->ap_ccif_base + APCCIF_CHDATA,
				(idx + 3) * sizeof(u32)));
	}
}

void __weak md_cd_get_md_bootup_status(struct ccci_modem *md,
	unsigned int *buff, int length)
{
	if (buff != NULL)
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"md_boot_stats0 / 1: weak func, maybe need add.\n");
	md_cd_dump_md_bootup_status(md);
}

static int md_cd_dump_info(struct ccci_modem *md,
	enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;

	if (flag & DUMP_FLAG_CCIF_REG) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump CCIF REG\n");
		md_cd_dump_ccif_reg(md);
	}
	if (flag & DUMP_FLAG_PCCIF_REG) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "ignore Dump PCCIF REG\n");
		/*md_cd_dump_pccif_reg(md);*/
	}
	if (flag & DUMP_FLAG_CCIF) {
		int i;
		unsigned int *dest_buff = NULL;
		unsigned char ccif_sram[CCCI_EE_SIZE_CCIF_SRAM] = { 0 };
		int sram_size = md->hw_info->sram_size;

		if (buff)
			dest_buff = (unsigned int *)buff;
		else
			dest_buff = (unsigned int *)ccif_sram;
		if (length < sizeof(ccif_sram) && length > 0) {
			CCCI_ERROR_LOG(md->index, TAG,
				"dump CCIF SRAM length illegal %d/%zu\n",
				length, sizeof(ccif_sram));
			dest_buff = (unsigned int *)ccif_sram;
		} else {
			length = sizeof(ccif_sram);
		}

		for (i = 0; i < length / sizeof(unsigned int); i++) {
			*(dest_buff + i) =
				ccif_read32(md_info->ap_ccif_base,
				APCCIF_CHDATA + (sram_size - length) +
				i * sizeof(unsigned int));
		}
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump CCIF SRAM (last %d bytes)\n", length);
		ccci_util_mem_dump(md->index,
			CCCI_DUMP_MEM_DUMP, dest_buff, length);
#ifdef DEBUG_FOR_CCB
		ccci_hif_dump_status(1 << CCIF_HIF_ID, DUMP_FLAG_CCIF, 0);
#endif
	}

	/*HIF related dump flag*/
	if (flag & (DUMP_FLAG_QUEUE_0_1 | DUMP_FLAG_QUEUE_0 |
		DUMP_FLAG_IRQ_STATUS | DUMP_FLAG_CLDMA))
		ccci_hif_dump_status(md->hif_flag, flag, length);

	if (flag & DUMP_FLAG_REG)
		md_cd_dump_debug_register(md);
	if (flag & DUMP_FLAG_SMEM_EXP) {
		struct ccci_smem_region *mdccci_dbg =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_MDCCCI_DBG);
		struct ccci_smem_region *mdss_dbg =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_MDSS_DBG);

		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump exception share memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			mdccci_dbg->base_ap_view_vir, mdccci_dbg->size);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			mdss_dbg->base_ap_view_vir, mdss_dbg->size);
	}
	if (flag & DUMP_FLAG_SMEM_CCISM) {
		struct ccci_smem_region *scp =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_CCISM_SCP);

		CCCI_MEM_LOG_TAG(md->index, TAG,
			"Dump CCISM share memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			scp->base_ap_view_vir, scp->size);
	}
	if (flag & DUMP_FLAG_SMEM_CCB_CTRL) {
		struct ccci_smem_region *ccb_ctl =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_CCB_CTRL);

		if (ccb_ctl) {
			CCCI_MEM_LOG_TAG(md->index, TAG,
				"Dump CCB CTRL share memory\n");
			ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
				ccb_ctl->base_ap_view_vir,
				32 * ccb_configs_len * 2);
		}
	}
	if (flag & DUMP_FLAG_SMEM_CCB_DATA) {
		int i, j;
		unsigned char *curr_ch_p;
		unsigned int *curr_p;
		struct ccci_smem_region *ccb_data =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_CCB_START);
		struct ccci_smem_region *dhl_raw =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_DHL);

		if (ccb_data && ccb_data->size != 0) {
			CCCI_MEM_LOG_TAG(md->index, TAG,
				"Dump CCB DATA share memory\n");
			curr_ch_p = ccb_data->base_ap_view_vir;
			curr_p = (unsigned int *)curr_ch_p;
			for (i = 0; i < ccb_configs_len; i++) {
				/* dump dl buffer */
				for (j = 0; j < ccb_configs[i].dl_buff_size /
					ccb_configs[i].dl_page_size;  j++) {
					ccci_dump_write(md->index,
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
					ccci_dump_write(md->index,
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
			CCCI_MEM_LOG_TAG(md->index, TAG,
				"Dump DHL RAW share memory\n");
			curr_ch_p = dhl_raw->base_ap_view_vir;
			curr_p = (unsigned int *)curr_ch_p;
			ccci_dump_write(md->index,
				CCCI_DUMP_MEM_DUMP, 0,
					"%p: %08X %08X %08X %08X\n",
					curr_p, *curr_p, *(curr_p + 1),
					*(curr_p + 2), *(curr_p + 3));
		}
	}
	if (flag & DUMP_FLAG_IMAGE) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD image memory\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(void *)md->mem_layout.md_bank0.base_ap_view_vir,
			MD_IMG_DUMP_SIZE);
	}
	if (flag & DUMP_FLAG_LAYOUT) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD layout struct\n");
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			&md->mem_layout, sizeof(struct ccci_mem_layout));
	}

	if (flag & DUMP_FLAG_SMEM_MDSLP) {
		struct ccci_smem_region *low_pwr =
			ccci_md_get_smem_by_user_id(md->index,
				SMEM_USER_RAW_DBM);

		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD SLP registers\n");
		ccci_cmpt_mem_dump(md->index, low_pwr->base_ap_view_vir,
			low_pwr->size);
	}
	if (flag & DUMP_FLAG_MD_WDT) {
		CCCI_MEM_LOG_TAG(md->index, TAG, "Dump MD RGU registers\n");
		md_cd_lock_modem_clock_src(1);
#ifdef BASE_ADDR_MDRSTCTL
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			md_info->md_rgu_base, 0x88);
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			(md_info->md_rgu_base + 0x200), 0x5c);
#else
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
			md_info->md_rgu_base, 0x30);
#endif
		md_cd_lock_modem_clock_src(0);
		CCCI_MEM_LOG_TAG(md->index, TAG, "wdt_enabled=%d\n",
			atomic_read(&md->wdt_enabled));
	}

	if (flag & DUMP_MD_BOOTUP_STATUS)
		md_cd_get_md_bootup_status(md, (unsigned int *)buff, length);

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
	/* CCCI_NORMAL_LOG(md->index, TAG,
	 * "ccb tx notify to core %d\n", core_id);
	 */
	switch (core_id) {
	case P_CORE:
#if (MD_GENERATION == 6292)
		md_cd_pccif_send(md, AP_MD_CCB_WAKEUP);
#else
		md_cd_ccif_send(md, AP_MD_CCB_WAKEUP);
#endif
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

static ssize_t md_cd_dump_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count = snprintf(buf, 256,
		"support: ccif cldma register smem image layout\n");
	return count;
}

static ssize_t md_cd_dump_store(struct ccci_modem *md,
	const char *buf, size_t count)
{
	enum MD_STATE md_state = ccci_fsm_get_md_state(md->index);
	/* echo will bring "xxx\n" here,
	 * so we eliminate the "\n" during comparing
	 */
	if (md_state != GATED && md_state != INVALID) {
		if (strncmp(buf, "ccif", count - 1) == 0)
			ccci_hif_dump_status(1 << CCIF_HIF_ID,
				DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF, 0);
		if ((strncmp(buf, "cldma", count - 1) == 0) &&
			(md->hif_flag&(1<<CLDMA_HIF_ID)))
			ccci_hif_dump_status(1 << CLDMA_HIF_ID,
				DUMP_FLAG_CLDMA, -1);
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
		if (strncmp(buf, "image", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_IMAGE, NULL, 0);
		if (strncmp(buf, "layout", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_LAYOUT, NULL, 0);
		if (strncmp(buf, "mdslp", count - 1) == 0)
			md->ops->dump_info(md, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		if (strncmp(buf, "dpmaif", count - 1) == 0)
			ccci_hif_dump_status(1<<DPMAIF_HIF_ID,
				DUMP_FLAG_REG, -1);
		if (strncmp(buf, "port", count - 1) == 0)
			ccci_port_dump_status(md->index);
	}
	return count;
}

static ssize_t md_cd_control_show(struct ccci_modem *md, char *buf)
{
	int count = 0;

	count = snprintf(buf, 256,
		"support: cldma_reset cldma_stop ccif_assert md_type trace_sample\n");
	return count;
}

static ssize_t md_cd_control_store(struct ccci_modem *md,
	const char *buf, size_t count)
{
	int size = 0;

	if (md->hif_flag&(1<<CLDMA_HIF_ID)) {
		if (strncmp(buf, "cldma_reset", count - 1) == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "reset CLDMA\n");
			md_cd_lock_cldma_clock_src(1);
			cldma_stop(CLDMA_HIF_ID);
			md_cd_clear_all_queue(CLDMA_HIF_ID, OUT);
			md_cd_clear_all_queue(CLDMA_HIF_ID, IN);
			cldma_reset(CLDMA_HIF_ID);
			cldma_start(CLDMA_HIF_ID);
			md_cd_lock_cldma_clock_src(0);
		}
		if (strncmp(buf, "cldma_stop", count - 1) == 0) {
			CCCI_NORMAL_LOG(md->index, TAG, "stop CLDMA\n");
			md_cd_lock_cldma_clock_src(1);
			cldma_stop(CLDMA_HIF_ID);
			md_cd_lock_cldma_clock_src(0);
		}
	}
	if (strncmp(buf, "ccif_assert", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG,
			"use CCIF to force MD assert\n");
		md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	if (strncmp(buf, "ccif_reset", count - 1) == 0) {
		struct md_sys1_info *md_info =
			(struct md_sys1_info *)md->private_data;

		CCCI_NORMAL_LOG(md->index, TAG, "reset AP MD1 CCIF\n");
		ccci_reset_ccif_hw(md->index, AP_MD1_CCIF,
			md_info->ap_ccif_base, md_info->md_ccif_base);
	}
	if (strncmp(buf, "ccci_trm", count - 1) == 0) {
		CCCI_NORMAL_LOG(md->index, TAG, "TRM triggered\n");
		/* ccci_md_send_msg_to_user(md, CCCI_MONITOR_CH,
		 * CCCI_MD_MSG_RESET_REQUEST, 0);
		 */
	}
	size = strlen("md_type=");
	if (strncmp(buf, "md_type=", size) == 0) {
		md->per_md_data.config.load_type_saving = buf[size] - '0';
		CCCI_NORMAL_LOG(md->index, TAG, "md_type_store %d\n",
			md->per_md_data.config.load_type_saving);
		/* ccci_md_send_msg_to_user(md, CCCI_MONITOR_CH,
		 * CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
		 */
	}
	size = strlen("trace_sample=");
	if (strncmp(buf, "trace_sample=", size) == 0) {
		trace_sample_time = (buf[size] - '0') * 100000000;
		CCCI_NORMAL_LOG(md->index, TAG,
			"trace_sample_time %u\n", trace_sample_time);
	}
	size = strlen("md_dbg_dump=");
	if (strncmp(buf, "md_dbg_dump=", size)
		== 0 && size < count - 1) {
		size = kstrtouint(buf+size, 16,
				&md->per_md_data.md_dbg_dump_flag);
		if (size)
			md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_ALL;
		CCCI_NORMAL_LOG(md->index, TAG, "md_dbg_dump 0x%X\n",
			md->per_md_data.md_dbg_dump_flag);
	}

	return count;
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

CCCI_MD_ATTR(NULL, dump, 0660, md_cd_dump_show, md_cd_dump_store);
CCCI_MD_ATTR(NULL, control, 0660, md_cd_control_show, md_cd_control_store);
CCCI_MD_ATTR(NULL, parameter, 0660, md_cd_parameter_show,
	md_cd_parameter_store);

static void md_cd_sysfs_init(struct ccci_modem *md)
{
	int ret;

	ccci_md_attr_dump.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_dump.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_dump.attr.name, ret);

	ccci_md_attr_control.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_control.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_control.attr.name, ret);

	ccci_md_attr_parameter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_parameter.attr);
	if (ret)
		CCCI_ERROR_LOG(md->index, TAG,
			"fail to add sysfs node %s %d\n",
			ccci_md_attr_parameter.attr.name, ret);
}

static struct syscore_ops ccci_modem_sysops = {
	.suspend = ccci_modem_syssuspend,
	.resume = ccci_modem_sysresume,
};

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
static u64 cldma_dmamask = DMA_BIT_MASK(36);
static int ccci_modem_probe(struct platform_device *plat_dev)
{
	struct ccci_modem *md;
	struct md_sys1_info *md_info;
	int md_id;
	struct ccci_dev_cfg dev_cfg;
	int ret;
	struct md_hw_info *md_hw;

	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md hw mem fail\n", __func__);
		return -1;
	}
	ret = md_cd_get_modem_hw_info(plat_dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get hw info fail(%d)\n", __func__, ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}

	/* Allocate md ctrl memory and do initialize */
	md = ccci_md_alloc(sizeof(struct md_sys1_info));
	if (md == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc modem ctrl mem fail\n", __func__);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}
	md->index = md_id = dev_cfg.index;
	md->per_md_data.md_capability = dev_cfg.capability;
	md->hw_info = md_hw;

	md->plat_dev = plat_dev;
	md->plat_dev->dev.dma_mask = &cldma_dmamask;
	md->plat_dev->dev.coherent_dma_mask = cldma_dmamask;
	md->ops = &md_cd_ops;
	CCCI_INIT_LOG(md_id, TAG,
		"%s:md=%p,md->private_data=%p\n", __func__,
		md, md->private_data);

	/* register modem */
	ccci_md_register(md);

	/* init modem private data */
	md_info = (struct md_sys1_info *)md->private_data;

	snprintf(md->trm_wakelock_name, sizeof(md->trm_wakelock_name),
		"md%d_cldma_trm", md_id + 1);
	md->trm_wake_lock = wakeup_source_register(md->trm_wakelock_name);
	if (!md->trm_wake_lock) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	snprintf(md_info->peer_wakelock_name,
		sizeof(md_info->peer_wakelock_name),
		"md%d_cldma_peer", md_id + 1);

	md_info->peer_wake_lock =
		wakeup_source_register(md_info->peer_wakelock_name);
	if (!md_info->peer_wake_lock) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	/* Copy HW info */
	md_info->ap_ccif_base = (void __iomem *)md_hw->ap_ccif_base;
	md_info->md_ccif_base = (void __iomem *)md_hw->md_ccif_base;
#if (MD_GENERATION <= 6292)
	md_info->ap_ccif_irq_id = md_hw->ap_ccif_irq_id;
#else
	md_info->ap_ccif_irq_id = md_hw->ap_ccif_irq1_id;
#endif
	md_info->channel_id = 0;
	atomic_set(&md_info->ccif_irq_enabled, 1);

	md->md_wdt_irq_id = md_hw->md_wdt_irq_id;
	atomic_set(&md->reset_on_going, 1);
	/* IRQ is default enabled after request_irq */
	atomic_set(&md->wdt_enabled, 1);

	ret = of_property_read_u32(plat_dev->dev.of_node,
		"mediatek,mdhif_type", &md->hif_flag);
	if (ret != 0)
		md->hif_flag = (1 << MD1_NET_HIF | 1 << MD1_NORMAL_HIF);
	ccci_hif_init(md->index, md->hif_flag);

	/* register SYS CORE suspend resume call back */
	register_syscore_ops(&ccci_modem_sysops);

	/* add sysfs entries */
	md_cd_sysfs_init(md);
	/* hook up to device */
	plat_dev->dev.platform_data = md;

	return 0;
}

static const struct dev_pm_ops ccci_modem_pm_ops = {
	.suspend = ccci_modem_pm_suspend,
	.resume = ccci_modem_pm_resume,
	.freeze = ccci_modem_pm_suspend,
	.thaw = ccci_modem_pm_resume,
	.poweroff = ccci_modem_pm_suspend,
	.restore = ccci_modem_pm_resume,
	.restore_noirq = ccci_modem_pm_restore_noirq,
};

#ifdef CONFIG_OF
#if (MD_GENERATION <= 6293)
static const struct of_device_id ccci_modem_of_ids[] = {
	{.compatible = "mediatek,mdcldma",},
	{}
};
#else
static const struct of_device_id ccci_modem_of_ids[] = {
	{.compatible = "mediatek,mddriver",},
	{}
};
#endif
#endif

static struct platform_driver ccci_modem_driver = {

	.driver = {
		   .name = "driver_modem",
#ifdef CONFIG_OF
		   .of_match_table = ccci_modem_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_modem_pm_ops,
#endif
		   },
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"clmda modem platform driver register fail(%d)\n",
			ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("CCCI modem driver v0.1");
MODULE_LICENSE("GPL");
