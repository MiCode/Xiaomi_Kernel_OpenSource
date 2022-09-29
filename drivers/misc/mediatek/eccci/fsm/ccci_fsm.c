// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * Author: Xiao Wang <xiao.wang@mediatek.com>
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <soc/mediatek/emi.h>

#include "ccci_fsm_internal.h"
#include "ccci_fsm_sys.h"
#include "md_sys1_platform.h"
#include "modem_sys.h"
#include "ccci_auxadc.h"

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif

struct ccci_fsm_ctl *ccci_fsm_entries;

static void fsm_finish_command(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, int result);
static void fsm_finish_event(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_event *event);

static int needforcestop;
static int hs2_done;
static int s_is_normal_mdee;
static int s_devapc_dump_counter;

static void (*s_md_state_cb)(enum MD_STATE old_state,
				enum MD_STATE new_state);

static void (*s_dpmaif_debug_push_data_to_stack)(void);

void ccci_set_dpmaif_debug_cb(void (*dpmaif_debug_cb)(void))
{
	s_dpmaif_debug_push_data_to_stack = dpmaif_debug_cb;
}
EXPORT_SYMBOL(ccci_set_dpmaif_debug_cb);


int mtk_ccci_register_md_state_cb(
		void (*md_state_cb)(
			enum MD_STATE old_state,
			enum MD_STATE new_state))
{
	s_md_state_cb = md_state_cb;

	return 0;
}
EXPORT_SYMBOL(mtk_ccci_register_md_state_cb);

int force_md_stop(struct ccci_fsm_monitor *monitor_ctl)
{
	int ret = -1;
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	needforcestop = 1;
	if (!ctl) {
		CCCI_ERROR_LOG(0, FSM,
			"fsm_append_command:CCCI_COMMAND_STOP fal\n");
		return -1;
	}
	ret = fsm_append_command(ctl, CCCI_COMMAND_STOP, 0);
	CCCI_NORMAL_LOG(0, FSM,
			"force md stop\n");
	return ret;
}

unsigned long __weak BAT_Get_Battery_Voltage(int polling_mode)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

void mdee_set_ex_time_str(unsigned int type, char *str)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (ctl == NULL) {
		CCCI_ERROR_LOG(0, FSM,
			"%s:ccci_fsm_entries is null\n", __func__);
		return;
	}
	mdee_set_ex_start_str(&ctl->ee_ctl, type, str);
}

static struct ccci_fsm_command *fsm_check_for_ee(struct ccci_fsm_ctl *ctl,
	int xip)
{
	struct ccci_fsm_command *cmd = NULL;
	struct ccci_fsm_command *next = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ctl->command_lock, flags);
	if (!list_empty(&ctl->command_queue)) {
		cmd = list_first_entry(&ctl->command_queue,
			struct ccci_fsm_command, entry);
		if (cmd->cmd_id == CCCI_COMMAND_EE) {
			if (xip)
				list_del(&cmd->entry);
			next = cmd;
		}
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	return next;
}

static inline int fsm_broadcast_state(struct ccci_fsm_ctl *ctl,
	enum MD_STATE state)
{
	enum MD_STATE old_state;

	if (unlikely(ctl->md_state != BOOT_WAITING_FOR_HS2 && state == READY)) {
		CCCI_NORMAL_LOG(0, FSM,
		"ignore HS2 when md_state=%d\n",
		ctl->md_state);
		return 0;
	}

	CCCI_NORMAL_LOG(0, FSM,
			"md_state change from %d to %d\n",
			ctl->md_state, state);

	old_state = ctl->md_state;
	ctl->md_state = state;

	/* update to port first,
	 * otherwise send message on HS2 may fail
	 */
	ccci_port_md_status_notify(state);
	ccci_hif_state_notification(state);
#ifdef CCCI_KMODULE_ENABLE
#ifdef FEATURE_SCP_CCCI_SUPPORT
	if (ctl->scp_ctl) {
		CCCI_NORMAL_LOG(0, FSM,
			"ccci scp state sync %d, %lx, %lx\n", state,
			(unsigned long)ctl->scp_ctl, (unsigned long)ctl->scp_ctl->md_state_sync);
		if (ctl->scp_ctl->md_state_sync)
			ctl->scp_ctl->md_state_sync(state);
		else {
			CCCI_NORMAL_LOG(0, FSM,
				"ccci scp_work not ready %d\n", state);
		}
	} else
		CCCI_NORMAL_LOG(0, FSM, "ccci scp not ready %d\n", state);
#endif
#endif
	if (old_state != state &&
		s_md_state_cb != NULL)
		s_md_state_cb(old_state, state);

	return 0;
}

static void fsm_routine_zombie(struct ccci_fsm_ctl *ctl)
{
	struct ccci_fsm_event *event = NULL;
	struct ccci_fsm_event *evt_next = NULL;
	struct ccci_fsm_command *cmd = NULL;
	struct ccci_fsm_command *cmd_next = NULL;
	unsigned long flags;

	CCCI_ERROR_LOG(0, FSM,
		"unexpected FSM state %d->%d, from %ps\n",
		ctl->last_state, ctl->curr_state,
		__builtin_return_address(0));
	spin_lock_irqsave(&ctl->command_lock, flags);
	list_for_each_entry_safe(cmd,
		cmd_next, &ctl->command_queue, entry) {
		CCCI_ERROR_LOG(0, FSM,
		"unhandled command %d\n", cmd->cmd_id);
		list_del(&cmd->entry);
		fsm_finish_command(ctl, cmd, -1);
	}
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event,
		evt_next, &ctl->event_queue, entry) {
		CCCI_ERROR_LOG(0, FSM,
		"unhandled event %d\n", event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
}

int ccci_fsm_is_normal_mdee(void)
{
	return s_is_normal_mdee;
}

int ccci_fsm_increase_devapc_dump_counter(void)
{
	return (++ s_devapc_dump_counter);
}

/* cmd is not NULL only when reason is ordinary EE */
static void fsm_routine_exception(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, enum CCCI_EE_REASON reason)
{
	int count = 0, ex_got = 0;
	int rec_ok_got = 0, pass_got = 0;
	struct ccci_fsm_event *event = NULL;
	unsigned long flags;

	CCCI_NORMAL_LOG(0, FSM,
		"exception %d, from %ps\n",
		reason, __builtin_return_address(0));
	fsm_monitor_send_message(CCCI_MD_MSG_EXCEPTION, 0);
	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED) {
		if (cmd)
			fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_EXCEPTION;
	if (reason == EXCEPTION_WDT
		|| reason == EXCEPTION_HS1_TIMEOUT
		|| reason == EXCEPTION_HS2_TIMEOUT)
		mdee_set_ex_start_str(&ctl->ee_ctl, 0, NULL);

	/* 2. check EE reason */
	switch (reason) {
	case EXCEPTION_HS1_TIMEOUT:
		CCCI_ERROR_LOG(0, FSM,
			"MD_BOOT_HS1_FAIL!\n");
		fsm_md_bootup_timeout_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_HS2_TIMEOUT:
		CCCI_ERROR_LOG(0, FSM,
			"MD_BOOT_HS2_FAIL!\n");
		fsm_md_bootup_timeout_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_MD_NO_RESPONSE:
		CCCI_ERROR_LOG(0, FSM,
			"MD_NO_RESPONSE!\n");
		fsm_broadcast_state(ctl, EXCEPTION);
		fsm_md_no_response_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_WDT:
		fsm_broadcast_state(ctl, EXCEPTION);
		CCCI_ERROR_LOG(0, FSM,
			"MD_WDT!\n");
		fsm_md_wdt_handler(&ctl->ee_ctl);
		break;
	case EXCEPTION_EE:
		if (s_dpmaif_debug_push_data_to_stack)
			s_dpmaif_debug_push_data_to_stack();

		fsm_broadcast_state(ctl, EXCEPTION);
		/* no need to implement another
		 * event polling in EE_CTRL,
		 * so we do it here
		 */
		ccci_md_exception_handshake(MD_EX_CCIF_TIMEOUT);
#ifdef ENABLE_EMIMPU_CB
		mtk_clear_md_violation();
#endif
		count = 0;
		while (count < MD_EX_REC_OK_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
					struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX) {
					ex_got = 1;
					fsm_finish_event(ctl, event);
				} else if (event->event_id ==
						CCCI_EVENT_MD_EX_REC_OK) {
					rec_ok_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (rec_ok_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		fsm_md_exception_stage(&ctl->ee_ctl, 1);
		count = 0;
		while (count < MD_EX_PASS_TIMEOUT/EVENT_POLL_INTEVAL) {
			spin_lock_irqsave(&ctl->event_lock, flags);
			if (!list_empty(&ctl->event_queue)) {
				event = list_first_entry(&ctl->event_queue,
					struct ccci_fsm_event, entry);
				if (event->event_id == CCCI_EVENT_MD_EX_PASS) {
					pass_got = 1;
					fsm_finish_event(ctl, event);
				}
			}
			spin_unlock_irqrestore(&ctl->event_lock, flags);
			if (pass_got)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		fsm_md_exception_stage(&ctl->ee_ctl, 2);
		break;
	default:
		break;
	}
	/* 3. always end in exception state */
	if (cmd)
		fsm_finish_command(ctl, cmd, 1);
}

static void append_runtime_feature(char **p_rt_data,
	struct ccci_runtime_feature *rt_feature, void *data)
{
	CCCI_DEBUG_LOG(-1, FSM,
		"append rt_data %p, feature %u len %u\n",
		*p_rt_data, rt_feature->feature_id,
		rt_feature->data_len);
	memcpy_toio(*p_rt_data, rt_feature,
		sizeof(struct ccci_runtime_feature));
	*p_rt_data += sizeof(struct ccci_runtime_feature);
	if (data != NULL) {
		memcpy_toio(*p_rt_data, data, rt_feature->data_len);
		*p_rt_data += rt_feature->data_len;
	}
}

/*
 *booting_start_id bit mapping:
 * |31---------16|15-----------8|7---------0|
 * | mdwait_time | logging_mode | boot_mode |
 * mdwait_time: getting from property at user space
 * logging_mode: usb/sd/idl mode, setting at user space
 * boot_mode: factory/meta/normal mode
 */
static unsigned int get_booting_start_id(struct ccci_modem *md)
{
	enum LOGGING_MODE mdlog_flag = MODE_IDLE;
	u32 booting_start_id;

	mdlog_flag = md->mdlg_mode & 0x0000ffff;
	booting_start_id = (((char)mdlog_flag << 8)
				| get_boot_mode_from_dts());
	booting_start_id |= md->mdlg_mode & 0xffff0000;

	CCCI_BOOTUP_LOG(0, FSM,
		"%s 0x%x\n", __func__, booting_start_id);
	return booting_start_id;
}

static void config_ap_side_feature(struct ccci_modem *md,
	struct md_query_ap_feature *md_feature)
{
	unsigned int udc_noncache_size = 0, udc_cache_size = 0;
#if (MD_GENERATION >= 6297)
	struct ccci_smem_region *region;
#endif

	md->runtime_version = AP_MD_HS_V2;
	md_feature->feature_set[BOOT_INFO].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[EXCEPTION_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

#ifdef FEATURE_SCP_CCCI_SUPPORT
	md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif

	md_feature->feature_set[CCISM_SHARE_MEMORY_EXP].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	if ((get_md_resv_phy_cap_size() > 0) || (get_md_resv_sib_size() > 0))
		md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	else
		md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[MD_CONSYS_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;

#if (MD_GENERATION >= 6297)
	region = ccci_md_get_smem_by_user_id(SMEM_USER_RAW_UDC_DESCTAB);
	if (region)
		udc_cache_size = region->size;
	else
		udc_cache_size = 0;

	region = ccci_md_get_smem_by_user_id(SMEM_USER_RAW_UDC_DATA);
	if (region)
		udc_noncache_size = region->size;
	else
		udc_noncache_size = 0;
	if (udc_noncache_size > 0 && udc_cache_size > 0)
		md_feature->feature_set[UDC_RAW_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	else
		md_feature->feature_set[UDC_RAW_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
#else
	get_md_resv_udc_info(&udc_noncache_size, &udc_cache_size);
	if (udc_noncache_size > 0 && udc_cache_size > 0)
		md_feature->feature_set[UDC_RAW_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	else
		md_feature->feature_set[UDC_RAW_SHARE_MEMORY].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
#endif
	if (get_smem_amms_pos_size() > 0)
		md_feature->feature_set[MD_POS_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_MUST_SUPPORT;
	else
		md_feature->feature_set[MD_POS_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;

	/* notice: CCB_SHARE_MEMORY should be set to support
	 * when at least one CCB region exists
	 */
	md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[LWA_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

	md_feature->feature_set[MISC_INFO_HIF_DMA_REMAP].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MULTI_MD_MPU].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

	/* default always support MISC_INFO_RTC_32K_LESS */
	CCCI_DEBUG_LOG(0, FSM, "MISC_32K_LESS support\n");
	md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

	md_feature->feature_set[MISC_INFO_RANDOM_SEED_NUM].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_GPS_COCLOCK].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_SBP_ID].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_CCCI].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_C2K].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MD_IMAGE_START_MEMORY].support_mask
		= CCCI_FEATURE_OPTIONAL_SUPPORT;
	md_feature->feature_set[EE_AFTER_EPOF].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[AP_CCMNI_MTU].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

#ifdef ENABLE_FAST_HEADER
	md_feature->feature_set[CCCI_FAST_HEADER].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#endif

	/* tire1 features */
#ifdef FEATURE_TC1_CUSTOMER_VAL
	md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[MISC_INFO_CUSTOMER_VAL].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef FEATURE_SYNC_C2K_MEID
	md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[MISC_INFO_C2K_MEID].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;


	if (md->hw_info->plat_val->md_gen >= 6295)
		md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_OPTIONAL_SUPPORT;
	else
		md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;

	md_feature->feature_set[MD_MTEE_SHARE_MEMORY_ENABLE].support_mask
		= CCCI_FEATURE_OPTIONAL_SUPPORT;

#if (MD_GENERATION >= 6297)
	md_feature->feature_set[MD_WIFI_PROXY_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_OPTIONAL_SUPPORT;
#else
	md_feature->feature_set[MD_WIFI_PROXY_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
#endif


#if (MD_GENERATION >= 6297)
	md_feature->feature_set[NVRAM_CACHE_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[NVRAM_CACHE_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef CCCI_SUPPORT_AP_MD_SECURE_FEATURE
	md_feature->feature_set[SECURITY_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;
#else
	/* This item is reserved */
	md_feature->feature_set[SECURITY_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
#endif
#if (MD_GENERATION >= 6297)
		md_feature->feature_set[AMMS_DRDI_COPY].support_mask =
			CCCI_FEATURE_MUST_SUPPORT;
#else
		md_feature->feature_set[AMMS_DRDI_COPY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
#endif

#if (MD_GENERATION >= 6297)
	md_feature->feature_set[MD_MEM_AP_VIEW_INF].support_mask =
		CCCI_FEATURE_OPTIONAL_SUPPORT;
#endif
}

#if (MD_GENERATION >= 6297)
static void ccci_sib_region_set_runtime(struct ccci_runtime_feature *rt_feature,
	struct ccci_runtime_share_memory *rt_shm)
{
	phys_addr_t md_sib_mem_addr;
	unsigned int md_sib_mem_size;

	get_md_sib_mem_info(&md_sib_mem_addr, &md_sib_mem_size);
	rt_feature->data_len =
		sizeof(struct ccci_runtime_share_memory);
	rt_shm->addr = 0;
	if (md_sib_mem_addr)
		rt_shm->size = md_sib_mem_size;
	else
		rt_shm->size = 0;
}

static void ccci_md_mem_inf_prepare(
		struct ccci_runtime_feature *rt_ft,
		struct ccci_runtime_md_mem_ap_addr *tbl, unsigned int num)
{
	unsigned int add_num = 0;
	phys_addr_t ro_rw_base, ncrw_base, crw_base;
	u32 ro_rw_size, ncrw_size, crw_size;
	int ret;

	ret = get_md_resv_mem_info(&ro_rw_base, &ro_rw_size,
					&ncrw_base, &ncrw_size);
	if (ret < 0) {
		CCCI_REPEAT_LOG(0, FSM, "%s get mdrorw and srw fail\n",
			__func__);
		return;
	}
	ret = get_md_resv_csmem_info(&crw_base, &crw_size);
	if (ret < 0) {
		CCCI_REPEAT_LOG(0, FSM, "%s get cache smem info fail\n",
			__func__);
		return;
	}

	/* Add bank 0 and bank 1 */
	if (add_num < num) {
		tbl[add_num].md_view_phy = 0;
		tbl[add_num].size = ro_rw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)ro_rw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(ro_rw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(0, FSM, "%s add bank0/1 fail(%d)\n",
			__func__, add_num);

	if (add_num < num) {
		tbl[add_num].md_view_phy = 0x40000000;
		tbl[add_num].size = ncrw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)ncrw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(ncrw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(0, FSM, "%s add bank4 nc fail(%d)\n",
			__func__, add_num);

	if (add_num < num) {
		tbl[add_num].md_view_phy = 0x40000000 +
				get_md_smem_cachable_offset();
		tbl[add_num].size = crw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)crw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(crw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(0, FSM, "%s add bank4 c fail(%d)\n",
			__func__, add_num);
	rt_ft->feature_id = MD_MEM_AP_VIEW_INF;
	rt_ft->data_len =
		(sizeof(struct ccci_runtime_md_mem_ap_addr)) * add_num;
}
#endif

static void ccci_smem_region_set_runtime(unsigned int id,
	struct ccci_runtime_feature *rt_feature,
	struct ccci_runtime_share_memory *rt_shm)
{
	struct ccci_smem_region *region = ccci_md_get_smem_by_user_id(id);

	if (region) {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = region->base_md_view_phy;
		rt_shm->size = region->size;
	} else {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = 0;
		rt_shm->size = 0;
	}
}

int ccci_md_prepare_runtime_data(unsigned char *data, int length)
{
	struct ccci_modem *md = ccci_get_modem();
	u8 i = 0;
	u32 total_len;
	int j;
	/*runtime data buffer */
	struct ccci_smem_region *region;
	struct ccci_smem_region *rt_data_region =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_RUNTIME_DATA);
	char *rt_data = (char *)rt_data_region->base_ap_view_vir;

	struct ccci_runtime_feature rt_feature;
	/*runtime feature type */
	struct ccci_runtime_share_memory rt_shm;
	struct ccci_misc_info_element rt_f_element;
	struct ccci_runtime_md_mem_ap_addr rt_mem_view[4];

	struct md_query_ap_feature *md_feature = NULL;
	struct md_query_ap_feature md_feature_ap;
	struct ccci_runtime_boot_info boot_info;
	unsigned int random_seed = 0;
	struct timespec64 t;
	unsigned int c2k_flags = 0;
	int adc_val = 0;

	CCCI_BOOTUP_LOG(0, FSM,
		"prepare_runtime_data  AP total %u features\n",
		MD_RUNTIME_FEATURE_ID_MAX);

	memset(&md_feature_ap, 0, sizeof(struct md_query_ap_feature));
	config_ap_side_feature(md, &md_feature_ap);

	md_feature = (struct md_query_ap_feature *)(data +
				sizeof(struct ccci_header));

	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		CCCI_BOOTUP_LOG(0, FSM,
			"md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			md_feature->head_pattern, md_feature->tail_pattern);
		return -1;
	}

	for (i = BOOT_INFO; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		memset(&rt_shm, 0, sizeof(struct ccci_runtime_share_memory));
		memset(&rt_f_element, 0, sizeof(struct ccci_misc_info_element));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT &&
		    md_feature_ap.feature_set[i].support_mask <
			CCCI_FEATURE_MUST_SUPPORT) {
			CCCI_BOOTUP_LOG(0, FSM,
				"feature %u not support for AP\n",
				rt_feature.feature_id);
			return -1;
		}

		CCCI_DEBUG_LOG(0, FSM,
			"ftr %u mask %u, ver %u\n",
			rt_feature.feature_id,
			md_feature->feature_set[i].support_mask,
			md_feature->feature_set[i].version);

		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version ==
			md_feature_ap.feature_set[i].version &&
			md_feature_ap.feature_set[i].support_mask >=
			CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >=
				md_feature_ap.feature_set[i].version) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		}

		if (rt_feature.support_info.support_mask ==
		CCCI_FEATURE_MUST_SUPPORT) {
			switch (rt_feature.feature_id) {
			case BOOT_INFO:
				memset(&boot_info, 0, sizeof(boot_info));
				rt_feature.data_len = sizeof(boot_info);
				boot_info.boot_channel = CCCI_CONTROL_RX;
				boot_info.booting_start_id =
					get_booting_start_id(md);
				adc_val = ccci_get_adc_mV();
				/* 0V ~ 0.1V is EVB */
				if (adc_val >= 100) {
					CCCI_BOOTUP_LOG(0, FSM,
					"ADC val:%d, Phone\n", adc_val);
					/* bit 1: 0: EVB 1: Phone */
					boot_info.boot_attributes |= (1 << 1);
				} else
					CCCI_BOOTUP_LOG(0, FSM,
					"ADC val:%d, EVB\n", adc_val);
				append_runtime_feature(&rt_data,
					&rt_feature, &boot_info);
				break;
			case EXCEPTION_SHARE_MEMORY:
				region = ccci_md_get_smem_by_user_id(
					SMEM_USER_RAW_MDCCCI_DBG);
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = region->base_md_view_phy;
				rt_shm.size = CCCI_EE_SMEM_TOTAL_SIZE;
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCIF_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_CCISM_MCU,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_CCISM_SCP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCB_SHARE_MEMORY:
				/* notice: we should add up
				 * all CCB region size here
				 */
				/* ctrl control first */
				region = ccci_md_get_smem_by_user_id(
					SMEM_USER_RAW_CCB_CTRL);
				if (region) {
					rt_feature.data_len =
					sizeof(struct ccci_misc_info_element);
					rt_f_element.feature[0] =
					region->base_md_view_phy;
					rt_f_element.feature[1] =
					region->size;
				}
				/* ccb data second */
				for (j = SMEM_USER_CCB_START;
					j <= SMEM_USER_CCB_END; j++) {
					region = ccci_md_get_smem_by_user_id(j);
					if (j == SMEM_USER_CCB_START
						&& region) {
						rt_f_element.feature[2] =
						region->base_md_view_phy;
						rt_f_element.feature[3] = 0;
					} else if (j == SMEM_USER_CCB_START
							&& region == NULL)
						break;
					if (region)
						rt_f_element.feature[3] +=
						region->size;
				}
				CCCI_BOOTUP_LOG(0, FSM,
					"ccb data size (include dsp raw): %X\n",
					rt_f_element.feature[3]);

				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case DHL_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_DHL,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case LWA_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_LWA,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case MULTI_MD_MPU:
				CCCI_BOOTUP_LOG(0, FSM,
				"new version md use multi-MPU.\n");
				md->multi_md_mpu_support = 1;
				rt_feature.data_len = 0;
				append_runtime_feature(&rt_data,
				&rt_feature, NULL);
				break;
			case DT_NETD_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_NETD,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case DT_USB_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_USB,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case SMART_LOGGING_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_SMART_LOGGING,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case MD1MD3_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_MD2MD,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;

			case MISC_INFO_HIF_DMA_REMAP:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RTC_32K_LESS:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RANDOM_SEED_NUM:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				get_random_bytes(&random_seed, sizeof(int));
				rt_f_element.feature[0] = random_seed;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_GPS_COCLOCK:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_SBP_ID:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = md->sbp_code;
				rt_f_element.feature[1] =
						get_soc_md_rt_rat();
				CCCI_BOOTUP_LOG(0, FSM,
					"sbp=0x%x,wmid[0x%x]\n",
					rt_f_element.feature[0],
					rt_f_element.feature[1]);
				CCCI_NORMAL_LOG(0, FSM,
					"sbp=0x%x,wmid[0x%x]\n",
					rt_f_element.feature[0],
					rt_f_element.feature[1]);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CCCI:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				/* sequence check */
				rt_f_element.feature[0] |= (1 << 0);
				/* polling MD status */
				rt_f_element.feature[0] |= (1 << 1);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CLIB_TIME:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				ktime_get_real_ts64(&t);
				/*set seconds information */
				rt_f_element.feature[0] =
				((unsigned int *)&t.tv_sec)[0];
				rt_f_element.feature[1] =
				((unsigned int *)&t.tv_sec)[1];
				/*sys_tz.tz_minuteswest; */
				rt_f_element.feature[2] = current_time_zone;
				/*not used for now */
				rt_f_element.feature[3] = sys_tz.tz_dsttime;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_C2K:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				c2k_flags = 0;

				if (check_rat_at_rt_setting("C"))
					c2k_flags |= (1 << 2);
				CCCI_NORMAL_LOG(0, FSM,
					"c2k_flags 0x%X; MD_GENERATION: %d\n",
					c2k_flags, MD_GENERATION);

				rt_f_element.feature[0] = c2k_flags;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MD_IMAGE_START_MEMORY:
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr =
				md->per_md_data.img_info[IMG_MD].address;
				rt_shm.size =
				md->per_md_data.img_info[IMG_MD].size;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case EE_AFTER_EPOF:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case AP_CCMNI_MTU:
				rt_feature.data_len =
				sizeof(unsigned int);
				random_seed =
				NET_RX_BUF - sizeof(struct ccci_header);
				append_runtime_feature(&rt_data,
				&rt_feature, &random_seed);
				break;
			case CCCI_FAST_HEADER:
				rt_feature.data_len = sizeof(unsigned int);
				random_seed = 1;
				append_runtime_feature(&rt_data,
				&rt_feature, &random_seed);
				break;
			case AUDIO_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_AUDIO,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY_EXP:
				ccci_smem_region_set_runtime(
					SMEM_USER_CCISM_MCU_EXP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_PHY_CAPTURE:
#if (MD_GENERATION >= 6297)
				ccci_sib_region_set_runtime(&rt_feature,
					&rt_shm);
#else
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_PHY_CAP,
					&rt_feature, &rt_shm);
#endif
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case UDC_RAW_SHARE_MEMORY:
				region = ccci_md_get_smem_by_user_id(
					SMEM_USER_RAW_UDC_DATA);
				if (region) {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[0] =
						region->base_md_view_phy;
					rt_f_element.feature[1] =
						region->size;
				} else {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[0] = 0;
					rt_f_element.feature[1] = 0;
				}
				region = ccci_md_get_smem_by_user_id(
					SMEM_USER_RAW_UDC_DESCTAB);
				if (region) {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[2] =
						region->base_md_view_phy;
					rt_f_element.feature[3] =
						region->size;
				} else {
					rt_feature.data_len = sizeof(
						struct ccci_misc_info_element);
					rt_f_element.feature[2] = 0;
					rt_f_element.feature[3] = 0;
				}
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_f_element);
				break;
			case MD_CONSYS_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_MD_CONSYS,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_USIP_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_USIP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_MTEE_SHARE_MEMORY_ENABLE:
				rt_feature.data_len = sizeof(unsigned int);
				/* use the random_seed as temp_u32 value */
				random_seed = get_mtee_is_enabled();
				append_runtime_feature(&rt_data, &rt_feature,
				&random_seed);
				break;
			case MD_POS_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_RAW_AMMS_POS,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			case MD_WIFI_PROXY_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_MD_WIFI_PROXY,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case NVRAM_CACHE_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_MD_NVRAM_CACHE,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_MEM_AP_VIEW_INF:
				ccci_md_mem_inf_prepare(&rt_feature,
					rt_mem_view, 4);
				append_runtime_feature(&rt_data, &rt_feature,
				rt_mem_view);
				break;
#ifdef CCCI_SUPPORT_AP_MD_SECURE_FEATURE
			case SECURITY_SHARE_MEMORY:
				ccci_smem_region_set_runtime(
					SMEM_USER_SECURITY_SMEM,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
#endif
			case AMMS_DRDI_COPY:
				ccci_smem_region_set_runtime(
					SMEM_USER_MD_DRDI,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			default:
				break;
			};
		} else {
			rt_feature.data_len = 0;
			append_runtime_feature(&rt_data, &rt_feature, NULL);
		}

	}

	total_len = rt_data - (char *)rt_data_region->base_ap_view_vir;
	CCCI_BOOTUP_DUMP_LOG(0, FSM, "AP runtime data\n");
	ccci_util_mem_dump(CCCI_DUMP_BOOTUP,
		rt_data_region->base_ap_view_vir, total_len);

	return 0;
}

static void fsm_routine_start(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	int ret;
	int count = 0, user_exit = 0, hs1_got = 0, hs2_got = 0;
	struct ccci_fsm_event *event = NULL;
	struct ccci_fsm_event  *next = NULL;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_READY)
		goto success;
	if (ctl->curr_state != CCCI_FSM_GATED) {
		fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STARTING;
	__pm_stay_awake(ctl->wakelock);
	/* 2. poll for critical users exit */
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL && !needforcestop) {
		if (ccci_port_check_critical_user() == 0 ||
				ccci_port_critical_user_only_fsd()) {
			user_exit = 1;
			break;
		}
		count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	/* what if critical user still alive:
	 * we can't wait for ever since this may be
	 * an illegal sequence (enter flight mode -> force start),
	 * and we must be able to recover from it.
	 * we'd better not entering exception state as
	 * start operation is not allowed in exception state.
	 * so we tango on...
	 */
	if (!user_exit)
		CCCI_ERROR_LOG(0, FSM, "critical user alive %d\n",
			ccci_port_check_critical_user());
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, next, &ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(0, FSM,
			"drop event %d before start\n", event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	/* 3. action and poll event queue */
	ccci_md_pre_start();
	fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS1);
	ret = ccci_md_start();
	if (ret)
		goto fail;
	ctl->boot_count++;
	count = 0;
	while (count < BOOT_TIMEOUT/EVENT_POLL_INTEVAL && !needforcestop) {
		spin_lock_irqsave(&ctl->event_lock, flags);
		if (!list_empty(&ctl->event_queue)) {
			event = list_first_entry(&ctl->event_queue,
						struct ccci_fsm_event, entry);
			if (event->event_id == CCCI_EVENT_HS1) {
				hs1_got = 1;
				fsm_broadcast_state(ctl, BOOT_WAITING_FOR_HS2);

				if (event->length
					== sizeof(struct md_query_ap_feature)
					+ sizeof(struct ccci_header))
					ccci_md_prepare_runtime_data(
						event->data, event->length);
				else if (event->length
						== sizeof(struct ccci_header))
					CCCI_NORMAL_LOG(0, FSM,
						"old handshake1 message\n");
				else
					CCCI_ERROR_LOG(0, FSM,
						"invalid MD_QUERY_MSG %d\n",
						event->length);
#ifdef SET_EMI_STEP_BY_STAGE
				ccci_set_mem_access_protection_second_stage();
#endif
				ccci_md_dump_info(DUMP_MD_BOOTUP_STATUS, NULL, 0);
				fsm_finish_event(ctl, event);

				spin_unlock_irqrestore(&ctl->event_lock, flags);
				/* this API would alloc skb */
				ret = ccci_md_send_runtime_data();
				CCCI_NORMAL_LOG(0, FSM,
					"send runtime data %d\n", ret);
				spin_lock_irqsave(&ctl->event_lock, flags);
			} else if (event->event_id == CCCI_EVENT_HS2) {
				hs2_got = 1;
				hs2_done = 1;

				fsm_broadcast_state(ctl, READY);

				fsm_finish_event(ctl, event);
			}
		}
		spin_unlock_irqrestore(&ctl->event_lock, flags);
		if (fsm_check_for_ee(ctl, 0)) {
			CCCI_ERROR_LOG(0, FSM,
				"early exception detected\n");
			goto fail_ee;
		}
		if (hs2_got)
			goto success;
		/* defeatured for now, just enlarge BOOT_TIMEOUT */
		if (atomic_read(&ctl->fs_ongoing))
			count = 0;
		else
			count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	if (needforcestop) {
		fsm_finish_command(ctl, cmd, -1);
		return;
	}
	/* 4. check result, finish command */
fail:
	if (hs1_got)
		fsm_routine_exception(ctl, NULL, EXCEPTION_HS2_TIMEOUT);
	else
		fsm_routine_exception(ctl, NULL, EXCEPTION_HS1_TIMEOUT);
	fsm_finish_command(ctl, cmd, -1);
	__pm_relax(ctl->wakelock);
	return;

fail_ee:
	/* exit imediately,
	 * let md_init have chance to start MD logger service
	 */
	fsm_finish_command(ctl, cmd, -1);
	__pm_relax(ctl->wakelock);
	return;

success:
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_READY;
	ccci_md_post_start();
	fsm_finish_command(ctl, cmd, 1);
	__pm_relax(ctl->wakelock);
	__pm_wakeup_event(ctl->wakelock, jiffies_to_msecs(10 * HZ));
}

static void fsm_routine_stop(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	struct ccci_fsm_event *event = NULL;
	struct ccci_fsm_event *next = NULL;
	struct ccci_fsm_command *ee_cmd = NULL;
	struct port_t *port = NULL;
	struct sk_buff *skb = NULL;
	unsigned long flags;

	/* 1. state sanity check */
	if (ctl->curr_state == CCCI_FSM_GATED)
		goto success;
	if (ctl->curr_state != CCCI_FSM_READY && !needforcestop
			&& ctl->curr_state != CCCI_FSM_EXCEPTION) {
		fsm_finish_command(ctl, cmd, -1);
		fsm_routine_zombie(ctl);
		return;
	}
	__pm_stay_awake(ctl->wakelock);
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_STOPPING;
	/* 2. pre-stop: polling MD for infinit sleep mode */
	ccci_md_pre_stop(
	cmd->flag & FSM_CMD_FLAG_FLIGHT_MODE
	?
	MD_FLIGHT_MODE_ENTER
	:
	MD_FLIGHT_MODE_NONE);
	/* 3. check for EE */
	ee_cmd = fsm_check_for_ee(ctl, 1);
	if (ee_cmd) {
		fsm_routine_exception(ctl, ee_cmd, EXCEPTION_EE);
		fsm_check_ee_done(&ctl->ee_ctl, EE_DONE_TIMEOUT);
	}
	/* to block port's write operation, must after EE flow done */
	fsm_broadcast_state(ctl, WAITING_TO_STOP);
	/*reset fsm poller*/
	ctl->poller_ctl.poller_state = FSM_POLLER_RECEIVED_RESPONSE;
	wake_up(&ctl->poller_ctl.status_rx_wq);
	/* 4. hardware stop */
	ccci_md_stop(
	cmd->flag & FSM_CMD_FLAG_FLIGHT_MODE
	?
	MD_FLIGHT_MODE_ENTER
	:
	MD_FLIGHT_MODE_NONE);
	/* 5. clear event queue */
	spin_lock_irqsave(&ctl->event_lock, flags);
	list_for_each_entry_safe(event, next,
		&ctl->event_queue, entry) {
		CCCI_NORMAL_LOG(0, FSM,
			"drop event %d after stop\n",
			event->event_id);
		fsm_finish_event(ctl, event);
	}
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	__pm_relax(ctl->wakelock);
	/* 6. always end in stopped state */
success:
	needforcestop = 0;
	/* when MD is stopped, the skb list of ccci_fs should be clean */
	port = port_get_by_channel(CCCI_FS_RX);
	if (port == NULL) {
		CCCI_ERROR_LOG(0, FSM, "port_get_by_channel fail");
		return;
	}

	if (port->flags & PORT_F_CLEAN) {
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
			ccci_free_skb(skb);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
	}
	ctl->last_state = ctl->curr_state;
	ctl->curr_state = CCCI_FSM_GATED;
	fsm_broadcast_state(ctl, GATED);
	fsm_finish_command(ctl, cmd, 1);
}

static int ccci_md_epon_set(void)
{
	struct ccci_modem *md = ccci_get_modem();
	struct ccci_smem_region *mdss_dbg
			= ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	int ret = 0, in_md_l2sram = 0;

	if (md->hw_info->md_l2sram_base) {
		md_cd_lock_modem_clock_src(1);
		ret = *((int *)(md->hw_info->md_l2sram_base
			+ md->hw_info->md_epon_offset)) == 0xBAEBAE10;
		md_cd_lock_modem_clock_src(0);
		in_md_l2sram = 1;
	} else if (mdss_dbg && mdss_dbg->base_ap_view_vir)
		ret = *((int *)(mdss_dbg->base_ap_view_vir
			+ md->hw_info->md_epon_offset)) == 0xBAEBAE10;

	CCCI_NORMAL_LOG(0, FSM, "reset MD after WDT, %s, 0x%x\n",
		(in_md_l2sram?"l2sram":"mdssdbg"), ret);

	return ret;
}

static void fsm_routine_wdt(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd)
{
	int reset_md = 0;
	int is_epon_set = 0;

	is_epon_set = ccci_md_epon_set();

	if (is_epon_set)
		reset_md = 1;
	else {
		if (ccci_port_get_critical_user(
				CRIT_USR_MDLOG) == 0) {
			CCCI_NORMAL_LOG(0, FSM,
				"mdlogger closed, reset MD after WDT\n");
			reset_md = 1;
		} else {
			fsm_routine_exception(ctl, NULL, EXCEPTION_WDT);
		}
	}
	if (reset_md)
		fsm_monitor_send_message(CCCI_MD_MSG_RESET_REQUEST, 0);

	fsm_finish_command(ctl, cmd, 1);
}

static int fsm_main_thread(void *data)
{
	struct ccci_fsm_ctl *ctl = (struct ccci_fsm_ctl *)data;
	struct ccci_fsm_command *cmd = NULL;
	unsigned long flags;
	int ret;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(ctl->command_wq,
			!list_empty(&ctl->command_queue));
		if (ret == -ERESTARTSYS)
			continue;
		spin_lock_irqsave(&ctl->command_lock, flags);
		cmd = list_first_entry(&ctl->command_queue,
			struct ccci_fsm_command, entry);
		/* delete first, otherwise hard to peek
		 * next command in routines
		 */
		list_del(&cmd->entry);
		spin_unlock_irqrestore(&ctl->command_lock, flags);

		CCCI_NORMAL_LOG(0, FSM, "command process\n");

		s_is_normal_mdee = 0;
		s_devapc_dump_counter = 0;

		switch (cmd->cmd_id) {
		case CCCI_COMMAND_START:
			fsm_routine_start(ctl, cmd);
			break;
		case CCCI_COMMAND_STOP:
			fsm_routine_stop(ctl, cmd);
			break;
		case CCCI_COMMAND_WDT:
			fsm_routine_wdt(ctl, cmd);
			break;
		case CCCI_COMMAND_EE:
			s_is_normal_mdee = 1;
			fsm_routine_exception(ctl, cmd, EXCEPTION_EE);
			break;
		case CCCI_COMMAND_MD_HANG:
			fsm_routine_exception(ctl, cmd,
				EXCEPTION_MD_NO_RESPONSE);
			break;
		default:
			fsm_finish_command(ctl, cmd, -1);
			fsm_routine_zombie(ctl);
			break;
		};
	}
	return 0;
}


int fsm_append_command(struct ccci_fsm_ctl *ctl,
	enum CCCI_FSM_COMMAND cmd_id, unsigned int flag)
{
	struct ccci_fsm_command *cmd = NULL;
	int result = 0;
	unsigned long flags;
	int ret;

	if (cmd_id <= CCCI_COMMAND_INVALID
			|| cmd_id >= CCCI_COMMAND_MAX) {
		CCCI_ERROR_LOG(0, FSM,
			"invalid command %d\n", cmd_id);
		return -CCCI_ERR_INVALID_PARAM;
	}
	cmd = kmalloc(sizeof(struct ccci_fsm_command),
		(in_irq() || in_softirq()
		|| irqs_disabled()) ? GFP_ATOMIC : GFP_KERNEL);
	if (!cmd) {
		CCCI_ERROR_LOG(0, FSM,
			"fail to alloc command %d\n", cmd_id);
		return -CCCI_ERR_GET_MEM_FAIL;
	}
	INIT_LIST_HEAD(&cmd->entry);
	init_waitqueue_head(&cmd->complete_wq);
	cmd->cmd_id = cmd_id;
	cmd->complete = 0;
	if (in_irq() || irqs_disabled())
		flag &= ~FSM_CMD_FLAG_WAIT_FOR_COMPLETE;
	cmd->flag = flag;

	spin_lock_irqsave(&ctl->command_lock, flags);
	list_add_tail(&cmd->entry, &ctl->command_queue);
	spin_unlock_irqrestore(&ctl->command_lock, flags);
	if (!in_irq())
		CCCI_NORMAL_LOG(0, FSM,
			"command %d is appended %x from %ps\n",
			cmd_id, flag,
			__builtin_return_address(0));
	/* after this line, only dereference cmd
	 * when "wait-for-complete"
	 */
	wake_up(&ctl->command_wq);
	if (flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		while (1) {
			ret = wait_event_interruptible(cmd->complete_wq,
				cmd->complete != 0);
			if (ret == -ERESTARTSYS)
				continue;
			if (cmd->complete != 1)
				result = -1;
			spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
			kfree(cmd);
			spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
			break;
		}
	}
	return result;
}

static void fsm_finish_command(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_command *cmd, int result)
{
	unsigned long flags;

	CCCI_NORMAL_LOG(0, FSM,
		"command %d is completed %d by %ps\n",
		cmd->cmd_id, result,
		__builtin_return_address(0));
	if (cmd->flag & FSM_CMD_FLAG_WAIT_FOR_COMPLETE) {
		spin_lock_irqsave(&ctl->cmd_complete_lock, flags);
		cmd->complete = result;
		/* do not dereference cmd after this line */
		wake_up_all(&cmd->complete_wq);
		/* after cmd in list,
		 * processing thread may see it
		 * without being waked up,
		 * so spinlock is needed
		 */
		spin_unlock_irqrestore(&ctl->cmd_complete_lock, flags);
	} else {
		/* no one is waiting for this cmd, free to free */
		kfree(cmd);
	}
}

int fsm_append_event(struct ccci_fsm_ctl *ctl, enum CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length)
{
	struct ccci_fsm_event *event = NULL;
	unsigned long flags;

	if (event_id <= CCCI_EVENT_INVALID || event_id >= CCCI_EVENT_MAX) {
		CCCI_ERROR_LOG(0, FSM, "invalid event %d\n", event_id);
		return -CCCI_ERR_INVALID_PARAM;
	}
	if (event_id == CCCI_EVENT_FS_IN) {
		atomic_set(&(ctl->fs_ongoing), 1);
		return 0;
	} else if (event_id == CCCI_EVENT_FS_OUT) {
		atomic_set(&(ctl->fs_ongoing), 0);
		return 0;
	}
	event = kmalloc(sizeof(struct ccci_fsm_event) + length,
		in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	if (!event) {
		CCCI_ERROR_LOG(0, FSM,
			"fail to alloc event%d\n", event_id);
		return -CCCI_ERR_GET_MEM_FAIL;
	}
	INIT_LIST_HEAD(&event->entry);
	event->event_id = event_id;
	event->length = length;
	if (data && length)
		memcpy(event->data, data, length);

	spin_lock_irqsave(&ctl->event_lock, flags);
	list_add_tail(&event->entry, &ctl->event_queue);
	spin_unlock_irqrestore(&ctl->event_lock, flags);
	/* do not derefence event after here */
	CCCI_NORMAL_LOG(0, FSM,
		"event %d is appended from %ps\n", event_id,
		__builtin_return_address(0));
	return 0;
}

/* must be called within protection of event_lock */
static void fsm_finish_event(struct ccci_fsm_ctl *ctl,
	struct ccci_fsm_event *event)
{
	list_del(&event->entry);
	CCCI_NORMAL_LOG(0, FSM,
		"event %d is completed by %ps\n", event->event_id,
		__builtin_return_address(0));
	kfree(event);
}

struct ccci_fsm_ctl *fsm_get_entity_by_device_number(dev_t dev_n)
{
	if (ccci_fsm_entries &&
		ccci_fsm_entries->monitor_ctl.dev_n == dev_n)
		return ccci_fsm_entries;

	return NULL;
}

struct ccci_fsm_ctl *fsm_get_entity(void)
{
	return ccci_fsm_entries;
}
EXPORT_SYMBOL(fsm_get_entity);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
void dump_md_info_in_devapc(struct ccci_modem *md)
{
	unsigned char ccif_sram[CCCI_EE_SIZE_CCIF_SRAM] = { 0 };
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);

	// DUMP_FLAG_CCIF_REG
	CCCI_MEM_LOG_TAG(0, FSM, "Dump CCIF REG\n");
	ccci_hif_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF_REG, NULL, -1);

	// DUMP_FLAG_CCIF
	ccci_hif_dump_status(1 << CCIF_HIF_ID, DUMP_FLAG_CCIF, ccif_sram,
			sizeof(ccif_sram));

	// DUMP_FLAG_QUEUE_0_1
	ccci_hif_dump_status(md->hif_flag, DUMP_FLAG_QUEUE_0_1, NULL, 0);

	// DUMP_FLAG_REG
	if (md->hw_info->plat_ptr->debug_reg)
		md->hw_info->plat_ptr->debug_reg(md, false);

	// DUMP_MD_BOOTUP_STATUS
	if (md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status(NULL, 0);

	// MD_DBG_DUMP_SMEM
	CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log\n");
	ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mdccci_dbg->base_ap_view_vir,
			mdccci_dbg->size);
	CCCI_MEM_LOG_TAG(0, FSM, "Dump mdss_dbg log\n");
	ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mdss_dbg->base_ap_view_vir,
			mdss_dbg->size);
	CCCI_MEM_LOG_TAG(0, FSM, "Dump mdl2sram log\n");
	if (md->hw_info->md_l2sram_base) {
		md_cd_lock_modem_clock_src(1);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, md->hw_info->md_l2sram_base,
			md->hw_info->md_l2sram_size);
		md_cd_lock_modem_clock_src(0);
	}
}

void ccci_dump_md_in_devapc(char *user_info)
{
	struct ccci_modem *md = NULL;

	CCCI_NORMAL_LOG(0, FSM, "%s called by %s\n", __func__, user_info);
	md = ccci_get_modem();
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, FSM, "%s dump start\n", __func__);
		dump_md_info_in_devapc(md);
	} else
		CCCI_NORMAL_LOG(0, FSM, "%s error, md is NULL!\n", __func__);
	CCCI_NORMAL_LOG(0, FSM, "%s exit\n", __func__);
}

static enum devapc_cb_status devapc_dump_adv_cb(uint32_t vio_addr)
{
	int count;

	CCCI_NORMAL_LOG(0, FSM,
		"[%s] vio_addr: 0x%x; is normal mdee: %d\n",
		__func__, vio_addr, ccci_fsm_is_normal_mdee());

	if (ccci_fsm_get_md_state() == EXCEPTION &&
		ccci_fsm_is_normal_mdee()) {
		count = ccci_fsm_increase_devapc_dump_counter();

		CCCI_NORMAL_LOG(0, FSM,
			"[%s] count: %d\n", __func__, count);

		if (count == 1)
			ccci_dump_md_in_devapc((char *)__func__);

		return DEVAPC_NOT_KE;

	} else {
		ccci_dump_md_in_devapc((char *)__func__);

		return DEVAPC_OK;
	}
}

static struct devapc_vio_callbacks devapc_md_vio_handle = {
	.id = INFRA_SUBSYS_MD,
	.debug_dump_adv = devapc_dump_adv_cb,
};
#endif

int ccci_fsm_init(void)
{
	struct ccci_fsm_ctl *ctl = NULL;
	int ret = 0;

	ctl = kzalloc(sizeof(struct ccci_fsm_ctl), GFP_KERNEL);
	if (ctl == NULL) {
		CCCI_ERROR_LOG(0, FSM,
					"%s kzalloc ccci_fsm_ctl fail\n",
					__func__);
		return -1;
	}

	ctl->last_state = CCCI_FSM_INVALID;
	ctl->curr_state = CCCI_FSM_GATED;
	INIT_LIST_HEAD(&ctl->command_queue);
	INIT_LIST_HEAD(&ctl->event_queue);
	init_waitqueue_head(&ctl->command_wq);
	spin_lock_init(&ctl->event_lock);
	spin_lock_init(&ctl->command_lock);
	spin_lock_init(&ctl->cmd_complete_lock);
	atomic_set(&ctl->fs_ongoing, 0);
	ret = snprintf(ctl->wakelock_name, sizeof(ctl->wakelock_name), "md_wakelock");
	if (ret <= 0 || ret >= sizeof(ctl->wakelock_name)) {
		CCCI_ERROR_LOG(0, FSM,
			"%s snprintf wakelock_name fail\n",
			__func__);
		ctl->wakelock_name[0] = 0;
	}
	ctl->wakelock = wakeup_source_register(NULL, ctl->wakelock_name);
	if (!ctl->wakelock) {
		CCCI_ERROR_LOG(0, FSM,
			"%s %d: init wakeup source fail",
			__func__, __LINE__);
		return -1;
	}
	ctl->fsm_thread = kthread_run(fsm_main_thread, ctl, "ccci_fsm");
#ifndef CCCI_KMODULE_ENABLE
#ifdef FEATURE_SCP_CCCI_SUPPORT
	fsm_scp_init(&ctl->scp_ctl);
#endif
#else
	CCCI_NORMAL_LOG(0, FSM, "%s oringinal position scp_init\n", __func__);
#endif
	fsm_poller_init(&ctl->poller_ctl);
	fsm_ee_init(&ctl->ee_ctl);
	fsm_monitor_init(&ctl->monitor_ctl);
	fsm_sys_init();

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_md_vio_handle);
#endif
	ccci_fsm_entries = ctl;
	return 0;
}

#ifdef CCCI_KMODULE_ENABLE
void ccci_fsm_scp_register(struct ccci_fsm_scp *scp_ctl)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (!ctl)
		return;

	ctl->scp_ctl = scp_ctl;
	CCCI_NORMAL_LOG(0, FSM,
		"ccci scp register to fsm, %lx, %lx, %lx\n",
		(unsigned long)ctl->scp_ctl,
		(unsigned long)scp_ctl,
		(unsigned long)&scp_ctl->md_state_sync);

}
EXPORT_SYMBOL(ccci_fsm_scp_register);
#endif
enum MD_STATE ccci_fsm_get_md_state(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (ctl)
		return ctl->md_state;
	else
		return INVALID;
}
EXPORT_SYMBOL(ccci_fsm_get_md_state);

enum MD_STATE_FOR_USER ccci_fsm_get_md_state_for_user(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (!ctl)
		return MD_STATE_INVALID;

	switch (ctl->md_state) {
	case INVALID:
	case WAITING_TO_STOP:
	case GATED:
		return MD_STATE_INVALID;
	case RESET:
	case BOOT_WAITING_FOR_HS1:
	case BOOT_WAITING_FOR_HS2:
		return MD_STATE_BOOTING;
	case READY:
		return MD_STATE_READY;
	case EXCEPTION:
		return MD_STATE_EXCEPTION;
	default:
		CCCI_ERROR_LOG(0, FSM,
			"Invalid md_state %d\n", ctl->md_state);
		return MD_STATE_INVALID;
	}
}
EXPORT_SYMBOL(ccci_fsm_get_md_state_for_user);


int ccci_fsm_recv_md_interrupt(enum MD_IRQ_TYPE type)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (!ctl)
		return -CCCI_ERR_INVALID_PARAM;

	__pm_wakeup_event(ctl->wakelock, jiffies_to_msecs(10 * HZ));

	if (type == MD_IRQ_WDT) {
		fsm_append_command(ctl, CCCI_COMMAND_WDT, 0);
	} else if (type == MD_IRQ_CCIF_EX) {
		fsm_md_exception_stage(&ctl->ee_ctl, 0);
		fsm_append_command(ctl, CCCI_COMMAND_EE, 0);
	}
	return 0;
}

int ccci_fsm_recv_control_packet(struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	int ret = 0, free_skb = 1;
	struct c2k_ctrl_port_msg *c2k_ctl_msg = NULL;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();

	if (!ctl)
		return -CCCI_ERR_INVALID_PARAM;

	CCCI_NORMAL_LOG(0, FSM,
		"control message 0x%X,0x%X\n",
		ccci_h->data[1], ccci_h->reserved);
	switch (ccci_h->data[1]) {
	case MD_INIT_START_BOOT: /* also MD_NORMAL_BOOT */
		if (ccci_h->reserved == MD_INIT_CHK_ID)
			fsm_append_event(ctl, CCCI_EVENT_HS1,
				skb->data, skb->len);
		else
			fsm_append_event(ctl, CCCI_EVENT_HS2, NULL, 0);
		break;
	case MD_EX:
	case MD_EX_REC_OK:
	case MD_EX_PASS:
	case CCCI_DRV_VER_ERROR:
		fsm_ee_message_handler(&ctl->ee_ctl, skb);
		break;

	case C2K_HB_MSG:
		free_skb = 0;
		ccci_fsm_recv_status_packet(skb);
		break;
	case C2K_STATUS_IND_MSG:
	case C2K_STATUS_QUERY_MSG:
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(0, FSM,
			"C2K line status %d: 0x%02x\n",
			ccci_h->data[1], c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)
			per_md_data->dtr_state = 1; /*connect */
		else
			per_md_data->dtr_state = 0; /*disconnect */
		break;
	case C2K_CCISM_SHM_INIT_ACK:
#ifndef CCCI_KMODULE_ENABLE
		fsm_ccism_init_ack_handler(ccci_h->reserved);
#endif
		break;
	case C2K_FLOW_CTRL_MSG:
		ccci_hif_start_queue(ccci_h->reserved, OUT);
		break;
	default:
		CCCI_ERROR_LOG(0, FSM,
			"unknown control message %x\n", ccci_h->data[1]);
		break;
	}

	if (free_skb)
		ccci_free_skb(skb);
	return ret;
}

/* requested by throttling feature */
unsigned long ccci_get_md_boot_count(void)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (ctl)
		return ctl->boot_count;
	else
		return 0;
}

unsigned int ccci_get_hs2_done_status(void)
{
	return hs2_done;
}

void reset_modem_hs2_status(void)
{
	hs2_done = 0;
}

