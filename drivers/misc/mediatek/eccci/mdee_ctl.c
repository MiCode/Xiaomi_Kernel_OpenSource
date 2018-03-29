/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/timer.h>
#include "ccci_config.h"

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "mdee_ctl.h"
#include "ccci_platform.h"

#ifndef DB_OPT_DEFAULT
#define DB_OPT_DEFAULT    (0)	/* Dummy macro define to avoid build error */
#endif

#ifndef DB_OPT_FTRACE
#define DB_OPT_FTRACE   (0)	/* Dummy macro define to avoid build error */
#endif

#define MAX_QUEUE_LENGTH 16
#define EX_TIMER_MD_HANG 5

unsigned int mdee_get_ee_type(struct md_ee *mdee)
{
	CCCI_NORMAL_LOG(mdee->md_id, KERN, "mdee_get_ee_type=%d\n", mdee->ex_type);
	return mdee->ex_type;
}
static inline void mdee_dump_ee_info(struct md_ee *mdee, MDEE_DUMP_LEVEL level, int more_info)
{
	if (mdee->ops->dump_ee_info)
		mdee->ops->dump_ee_info(mdee, level, more_info);
}
static inline void mdee_set_ee_pkg(struct md_ee *mdee, char *data, int len)
{
	if (mdee->ops->set_ee_pkg)
		mdee->ops->set_ee_pkg(mdee, data, len);
}
void mdee_bootup_timeout_handler(struct md_ee *mdee)
{
	int md_id = mdee->md_id;
	struct ccci_mem_layout *mem_layout = ccci_md_get_mem(mdee->md_obj);

	CCCI_NORMAL_LOG(md_id, KERN, "Dump MD image memory\n");
	ccci_mem_dump(md_id, (void *)mem_layout->md_region_vir, MD_IMG_DUMP_SIZE);
	CCCI_NORMAL_LOG(md_id, KERN, "Dump MD layout struct\n");
	ccci_mem_dump(md_id, mem_layout, sizeof(struct ccci_mem_layout));
	CCCI_NORMAL_LOG(md_id, KERN, "Dump queue 0 & 1\n");
	ccci_md_dump_info(mdee->md_obj, (DUMP_FLAG_QUEUE_0_1 | DUMP_MD_BOOTUP_STATUS), NULL, 0);
	CCCI_NORMAL_LOG(md_id, KERN, "Dump MD ee boot failed info\n");
	mdee_dump_ee_info(mdee, MDEE_DUMP_LEVEL_BOOT_FAIL, 0);
}

int mdee_ctlmsg_handler(struct ccci_port *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct md_ee *mdee = port_proxy_get_mdee(port->port_proxy);
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	unsigned long flags;
	int ret = 1;
	char need_update_state = 0;

	if (ccci_h->data[1] == MD_EX) {
		if (unlikely(ccci_h->reserved != MD_EX_CHK_ID)) {
			CCCI_ERROR_LOG(md_id, KERN, "receive invalid MD_EX\n");
		} else {
			spin_lock_irqsave(&mdee->ctrl_lock, flags);
			mdee->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_MSG_GET | MD_STATE_UPDATE | MD_EE_TIME_OUT_SET);
			spin_unlock_irqrestore(&mdee->ctrl_lock, flags);
			if (!(mdee->ee_info_flag & MD_EE_SWINT_GET))
				ccci_md_broadcast_state(mdee->md_obj, EXCEPTION);
			port_proxy_send_msg_to_md(port->port_proxy, CCCI_CONTROL_TX, MD_EX, MD_EX_CHK_ID, 1);
			port_proxy_append_fsm_event(port->port_proxy, CCCI_EVENT_MD_EX, NULL, 0);
		}
	} else if (ccci_h->data[1] == MD_EX_REC_OK) {
		if (unlikely
		    (ccci_h->reserved != MD_EX_REC_OK_CHK_ID
		     || skb->len < sizeof(struct ccci_header))) {
			CCCI_ERROR_LOG(md_id, KERN, "receive invalid MD_EX_REC_OK, resv=%x, len=%d\n",
				     ccci_h->reserved, skb->len);
		} else {
			spin_lock_irqsave(&mdee->ctrl_lock, flags);
			mdee->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_OK_MSG_GET);
			if ((mdee->ee_info_flag & MD_STATE_UPDATE) == 0) {
				mdee->ee_info_flag |= MD_STATE_UPDATE;
				mdee->ee_info_flag &= ~(MD_EE_TIME_OUT_SET);
				need_update_state = 1;
			}
			spin_unlock_irqrestore(&mdee->ctrl_lock, flags);
			if (!(mdee->ee_info_flag & MD_EE_SWINT_GET))
				ccci_md_broadcast_state(mdee->md_obj, EXCEPTION);
			/* Keep exception info package from MD*/
			mdee_set_ee_pkg(mdee, skb_pull(skb, sizeof(struct ccci_header)),
				skb->len - sizeof(struct ccci_header));

			port_proxy_append_fsm_event(port->port_proxy, CCCI_EVENT_MD_EX_REC_OK, NULL, 0);
		}
	} else if (ccci_h->data[1] == MD_EX_PASS) {
		spin_lock_irqsave(&mdee->ctrl_lock, flags);
		mdee->ee_info_flag |= MD_EE_PASS_MSG_GET;
		/* dump share memory again to let MD check exception flow */
		spin_unlock_irqrestore(&mdee->ctrl_lock, flags);
		port_proxy_append_fsm_event(port->port_proxy, CCCI_EVENT_MD_EX_PASS, NULL, 0);
	} else if (ccci_h->data[1] == CCCI_DRV_VER_ERROR) {
		CCCI_ERROR_LOG(md_id, KERN, "AP/MD driver version mis-match\n");
#if defined(CONFIG_MTK_AEE_FEATURE)
		aed_md_exception_api(NULL, 0, NULL, 0,
				"AP/MD driver version mis-match\n", DB_OPT_DEFAULT);
#endif

	} else {
		ret = 0;
	}
	return ret;
}

void mdee_monitor_func(struct md_ee *mdee)
{
	int ee_case;
	unsigned long flags;
	unsigned int ee_info_flag = 0;
	unsigned int md_dump_flag = 0;
	int md_id = mdee->md_id;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);
	struct ccci_mem_layout *mem_layout = ccci_md_get_mem(mdee->md_obj);

#if defined(CONFIG_MTK_AEE_FEATURE)
	CCCI_NORMAL_LOG(md_id, KERN, "MD exception timer 1:disable tracing\n");
	tracing_off();
#endif

	CCCI_MEM_LOG_TAG(md_id, KERN, "MD exception timer 1! ee=%x\n", mdee->ee_info_flag);
	spin_lock_irqsave(&mdee->ctrl_lock, flags);
	if (MD_EE_DUMP_ON_GOING & mdee->ee_info_flag) {
		spin_unlock_irqrestore(&mdee->ctrl_lock, flags);
		return;
	}
	mdee->ee_info_flag |= MD_EE_DUMP_ON_GOING;
	ee_info_flag = mdee->ee_info_flag;
	spin_unlock_irqrestore(&mdee->ctrl_lock, flags);

	if ((ee_info_flag & (MD_EE_MSG_GET | MD_EE_OK_MSG_GET | MD_EE_SWINT_GET)) ==
	    (MD_EE_MSG_GET | MD_EE_OK_MSG_GET | MD_EE_SWINT_GET)) {
		ee_case = MD_EE_CASE_NORMAL;
		CCCI_DEBUG_LOG(md_id, KERN, "Recv SWINT & MD_EX & MD_EX_REC_OK\n");
	} else if (!(ee_info_flag & MD_EE_SWINT_GET)
		   && (ee_info_flag & (MD_EE_MSG_GET | MD_EE_OK_MSG_GET))) {
		ee_case = MD_EE_CASE_SWINT_MISSING;
		CCCI_NORMAL_LOG(md_id, KERN, "SWINT missing, ee_info_flag=%x\n", ee_info_flag);
	} else if (ee_info_flag & MD_EE_MSG_GET) {
		ee_case = MD_EE_CASE_ONLY_EX;
		CCCI_NORMAL_LOG(md_id, KERN, "Only recv SWINT & MD_EX.\n");
	} else if (ee_info_flag & MD_EE_OK_MSG_GET) {
		ee_case = MD_EE_CASE_ONLY_EX_OK;
		CCCI_NORMAL_LOG(md_id, KERN, "Only recv SWINT & MD_EX_OK\n");
	} else if (ee_info_flag & MD_EE_SWINT_GET) {
		ee_case = MD_EE_CASE_ONLY_SWINT;
		CCCI_NORMAL_LOG(md_id, KERN, "Only recv SWINT.\n");
	} else if (ee_info_flag & MD_EE_PENDING_TOO_LONG) {
		ee_case = MD_EE_CASE_NO_RESPONSE;
	} else if (ee_info_flag & MD_EE_WDT_GET) {
		ee_case = MD_EE_CASE_WDT;
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "Invalid MD_EX, ee_info=%x\n", ee_info_flag);
		goto _dump_done;
	}

	mdee->ee_case = ee_case;
	if (!(ee_info_flag & MD_EE_SWINT_GET))
		ccci_md_broadcast_state(mdee->md_obj, EXCEPTION);

	/* Dump MD EE info */
	CCCI_MEM_LOG_TAG(md_id, KERN, "Dump MD EX log\n");
	/*parse & dump md ee info*/
	mdee_dump_ee_info(mdee, MDEE_DUMP_LEVEL_TIMER1, ee_case);

	/* Dump MD register*/
	md_dump_flag = DUMP_FLAG_REG;
	if (ee_case == MD_EE_CASE_ONLY_SWINT)
		md_dump_flag |= (DUMP_FLAG_QUEUE_0 | DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG);
	ccci_md_dump_info(mdee->md_obj, md_dump_flag, NULL, 0);
	/* check this first, as we overwrite share memory here */
	if (ee_case == MD_EE_CASE_NO_RESPONSE)
		ccci_md_dump_info(mdee->md_obj, DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG,
				   smem_layout->ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_CCIF_SRAM,
				   CCCC_SMEM_CCIF_SRAM_SIZE);

	/* Dump MD image memory */
	CCCI_MEM_LOG_TAG(md_id, KERN, "Dump MD image memory\n");
	ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, (void *)mem_layout->md_region_vir, MD_IMG_DUMP_SIZE);
	/* Dump MD memory layout */
	CCCI_MEM_LOG_TAG(md_id, KERN, "Dump MD layout struct\n");
	ccci_util_mem_dump(md_id, CCCI_DUMP_MEM_DUMP, mem_layout, sizeof(struct ccci_mem_layout));

	spin_lock_irqsave(&mdee->ctrl_lock, flags);
	if (MD_EE_PASS_MSG_GET & mdee->ee_info_flag)
		CCCI_ERROR_LOG(md_id, KERN, "MD exception timer 2 has been set!\n");
	spin_unlock_irqrestore(&mdee->ctrl_lock, flags);
	CCCI_ERROR_LOG(md_id, KERN, "MD exception timer 1:end\n");
 _dump_done:
	return;
}

void mdee_monitor2_func(struct md_ee *mdee)
{
	int md_id = mdee->md_id;
	unsigned long flags;
	int md_wdt_ee = 0;
	int ee_on_going = 0;
	struct ccci_modem *another_md;
	unsigned int md_dump_flag = 0;
	struct ccci_smem_layout *smem_layout = ccci_md_get_smem(mdee->md_obj);

	CCCI_ERROR_LOG(md_id, KERN, "MD exception timer 2! ee=%x\n", mdee->ee_info_flag);
	CCCI_MEM_LOG_TAG(md_id, KERN, "MD exception timer 2!\n");

	spin_lock_irqsave(&mdee->ctrl_lock, flags);
	if (MD_EE_TIMER2_DUMP_ON_GOING & mdee->ee_info_flag)
		ee_on_going = 1;
	else
		mdee->ee_info_flag |= MD_EE_TIMER2_DUMP_ON_GOING;
	if (MD_EE_WDT_GET & mdee->ee_info_flag)
		md_wdt_ee = 1;
	spin_unlock_irqrestore(&mdee->ctrl_lock, flags);

	if (ee_on_going)
		return;

	/* Dump MD register, only NO response case dump */
	if (mdee->ee_case == MD_EE_CASE_NO_RESPONSE)
		md_dump_flag = DUMP_FLAG_REG;
	if (mdee->ee_case == MD_EE_CASE_ONLY_SWINT)
		md_dump_flag |= (DUMP_FLAG_QUEUE_0 | DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG);
	ccci_md_dump_info(mdee->md_obj, md_dump_flag, NULL, 0);
	/* check this first, as we overwrite share memory here */
	if (mdee->ee_case == MD_EE_CASE_NO_RESPONSE)
		ccci_md_dump_info(mdee->md_obj, DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG,
				   smem_layout->ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_CCIF_SRAM,
				   CCCC_SMEM_CCIF_SRAM_SIZE);

	/*parse & dump md ee info*/
	mdee_dump_ee_info(mdee, MDEE_DUMP_LEVEL_TIMER2, mdee->ee_case);

	/* Dump another modem if necessary*/
	another_md = ccci_md_get_another(md_id);
	if (another_md && ccci_md_get_state(another_md) == BOOT_WAITING_FOR_HS2)
		ccci_md_dump_info(another_md, DUMP_FLAG_CCIF, NULL, 0);

	spin_lock_irqsave(&mdee->ctrl_lock, flags);
	/*
	* this flag should be the last action of a regular exception flow,
	* clear flag for reset MD later
	*/
	mdee->ee_info_flag = 0;
	spin_unlock_irqrestore(&mdee->ctrl_lock, flags);

	CCCI_MEM_LOG_TAG(md_id, KERN, "Enable WDT at exception exit.\n");
	ccci_md_ee_callback(mdee->md_obj, EE_FLAG_ENABLE_WDT);

	if (md_wdt_ee && md_id == MD_SYS3) {
		CCCI_ERROR_LOG(md_id, KERN, "trigger force assert after WDT EE.\n");
		ccci_md_force_assert(mdee->md_obj, MD_FORCE_ASSERT_BY_MD_WDT, NULL, 0);
	}
	CCCI_ERROR_LOG(md_id, KERN, "MD exception timer 2:end\n");
}

void mdee_state_notify(struct md_ee *mdee, MD_EX_STAGE stage)
{
	int md_id = mdee->md_id;
	unsigned long flags;

	spin_lock_irqsave(&mdee->ctrl_lock, flags);
	CCCI_NORMAL_LOG(md_id, KERN, "MD exception logical %d->%d\n", mdee->ex_stage, stage);
	mdee->ex_stage = stage;
	switch (mdee->ex_stage) {
	case EX_INIT:
		ccci_md_ee_callback(mdee->md_obj, EE_FLAG_DISABLE_WDT);
		mdee->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_SWINT_GET);
		ccci_fsm_append_command(mdee->md_obj, CCCI_COMMAND_EE, 0);
		ccci_md_broadcast_state(mdee->md_obj, EXCEPTION);
		break;
	case EX_DHL_DL_RDY:
		break;
	case EX_INIT_DONE:
		ccci_reset_seq_num(mdee->md_obj);
		break;
	case MD_NO_RESPONSE:
		/* don't broadcast exception state, only dump */
		CCCI_ERROR_LOG(md_id, KERN, "MD long time no response\n");
		mdee->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_PENDING_TOO_LONG | MD_STATE_UPDATE);
		ccci_fsm_append_command(mdee->md_obj, CCCI_COMMAND_MD_HANG, 0);
		break;
	case MD_WDT:
		mdee->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_WDT_GET | MD_STATE_UPDATE);
		ccci_fsm_append_command(mdee->md_obj, CCCI_COMMAND_WDT, 0);
		break;
	case MD_BOOT_TIMEOUT:
		mdee_bootup_timeout_handler(mdee);
		break;
	default:
		break;
	};
	spin_unlock_irqrestore(&mdee->ctrl_lock, flags);

}

struct md_ee *mdee_alloc(int md_id, void *md_obj)
{
	int ret = 0;
	struct md_ee *mdee = NULL;

	/* Allocate port_proxy obj and set all member zero */
	mdee = kzalloc(sizeof(struct md_ee), GFP_KERNEL);
	if (mdee == NULL) {
		CCCI_ERROR_LOG(md_id, KERN, "%s:alloc md_ee fail\n", __func__);
		return NULL;
	}
	mdee->md_id = md_id;
	mdee->md_obj = md_obj;
	if (md_id == MD_SYS1) {
#ifdef MD_UMOLY_EE_SUPPORT
		ret = mdee_dumper_v2_alloc(mdee);
#else
		ret = mdee_dumper_v1_alloc(mdee);
#endif
	} else if (md_id == MD_SYS3) {
		ret = mdee_dumper_v1_alloc(mdee);
	}

	spin_lock_init(&mdee->ctrl_lock);

	if (ret != 0)
		return NULL;
	return mdee;
}
