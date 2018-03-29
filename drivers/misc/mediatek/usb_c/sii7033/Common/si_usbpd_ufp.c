/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include <Wrap.h>
#include "si_time.h"
#include "si_usbpd_regs.h"
#include "si_usbpd_main.h"

#define UFP 2

/*********************************************************************/
int sii_usbpd_snk_alt_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected)
		return -ENODEV;

	if (pUsbpd->at_mode_established || pUsbpd->alt_mode_req_send) {
		pr_info("\n Already in MHL Mode\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SNK_Ready) {
		pUsbpd->alt_mode_req_send = true;
		wakeup_ufp_queue(pUsbpd->drv_context);
		pr_info("\n Alt mode initiated\n");
		return true;
	}
	pr_info("\n!!!!PD is not established!!!!!\n");
	return -EINVAL;
}

int sii_usbpd_snk_pwr_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("\n !!!!!PD is not Established!!!!\n");
		return -ENODEV;
	}

	if (pUsbpd->pr_swap.req_send || pUsbpd->pr_swap.req_rcv) {
		pr_info("\n pr_swap in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SNK_Ready) {
		pUsbpd->pr_swap.req_send = true;
		pUsbpd->pr_swap_state = PE_SNK_Swap_Init;
		wakeup_ufp_queue(pUsbpd->drv_context);
		pr_info("\n pr_swap initiated\n");
		return true;
	} else {
		return -EINVAL;
	}
	return -EINVAL;
}

int sii_usbpd_snk_data_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("\n !!!!!PD is not Established!!!!\n");
		return -ENODEV;
	}

	if (pUsbpd->dr_swap.req_send) {
		pr_info("\n dr_swap in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SNK_Ready) {
		pUsbpd->dr_swap.req_send = true;
		pUsbpd->dr_swap_state = PE_SNK_DR_Swap_Init;
		wakeup_ufp_queue(pUsbpd->drv_context);
		pr_info("\ndr_swap initiated\n");
		return true;
	} else {
		return -EINVAL;
	}
	return -EINVAL;
}

int sii_usbpd_give_src_cap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("\n !!!!!PD is not Established!!!!\n");
		return -ENODEV;
	}

	if (pUsbpd->src_cap_req) {
		pr_info("\n src cap req in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SNK_Ready) {
		pUsbpd->src_cap_req = true;
		wakeup_ufp_queue(pUsbpd->drv_context);
		pr_info("\n src_cap_req initiated\n");
		return true;
	} else {
		return -EINVAL;
	}
}

/**************************Timers***************************/
static void ps_source_off_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("\n$$ps_source_off_timer_handler $$\n");

		if (pdev->pr_swap_state == PE_SWAP_Wait_Ps_Rdy_Received) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SNK_Hard_Reset;
				goto success;
			}
		}
		goto exit;
success:	wakeup_ufp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void ps_transition_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("\n$$ Ps transition Timer $$\n");
		if (pdev->state == PE_SNK_Transition_Sink) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SNK_Hard_Reset;
				goto success;
			}
		}
		goto exit;
success:	wakeup_ufp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void sink_sender_response_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("\n$$ Sink Sender Response Timer $$\n");

		if (pdev->state == PE_SNK_Transition_Sink) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SNK_Hard_Reset;
				goto success;
			}
		}
		if (pdev->dr_swap_state == PE_PRS_Wait_Accept_DR_Swap) {
			pdev->dr_swap_state = PE_DRS_Swap_exit;
			goto success;
		}
		if (pdev->pr_swap_state == PE_PRS_Wait_Accept_PR_Swap) {
			pdev->pr_swap_state = PE_PRS_Swap_exit;
			goto success;
		}
		goto exit;
success:	wakeup_ufp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void sink_wait_captimer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	pr_info("\n$$ Sink Wait Timer $$\n");

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		if (pdev->state == PE_SNK_Wait_for_Capabilities) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SNK_Hard_Reset;
				goto success;
			}
		}
		goto exit;
success:	wakeup_ufp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void sink_activity_timer_handler(void *context)
{
	/*optional timer */
}

bool usbpd_sink_timer_create(struct sii_usbp_policy_engine *pUsbpd)
{
	int status = 0;

	if (!pUsbpd) {
		pr_warn("%s: failed in timer init.\n", __func__);
		return false;
	}
	if (!pUsbpd->usbpd_inst_sink_act_tmr) {
		status = sii_timer_create(sink_activity_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_sink_act_tmr,
					  SinkActivityTimer, false);

		if (status != 0) {
			pr_err("Failed to register SinkActivityTimer timer!\n");
			return false;
		}
	}
	if (!pUsbpd->usbpd_inst_sink_wait_cap_tmr) {
		status = sii_timer_create(sink_wait_captimer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_sink_wait_cap_tmr,
					  2500, false);

		if (status != 0) {
			pr_warn("Failed to register sink_wait_cap_tmr!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_inst_snk_sendr_resp_tmr) {
		status = sii_timer_create(sink_sender_response_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_snk_sendr_resp_tmr,
					  SenderResponseTimer, false);
		if (status != 0) {
			pr_warn("Failed to register SenderResponseTimer timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_inst_ps_trns_tmr) {
		status = sii_timer_create(ps_transition_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_ps_trns_tmr,
					  PSTransitionTimer, true);

		if (status != 0) {
			pr_warn("Failed to register PSTransitionTimer timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_ps_source_off_tmr) {
		status = sii_timer_create(ps_source_off_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_ps_source_off_tmr,
					  PSSourceOffTimer, false);

		if (status != 0) {
			pr_warn("Failed to register PSSourceOffTimer!\n");
			goto exit;
		}
	}
	return true;
exit:	usbpd_sink_timer_delete(pUsbpd);
	return false;
}


bool usbpd_sink_timer_delete(struct sii_usbp_policy_engine *pUsbpd)
{
	int rc = 0;

	if (pUsbpd->usbpd_ps_source_off_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_ps_source_off_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_ps_source_off_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_ps_src_off_tmr!\n");
			return false;
		}
	}
	if (pUsbpd->usbpd_inst_ps_trns_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_ps_trns_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_ps_trns_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_ps_trns_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_snk_sendr_resp_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_snk_sendr_resp_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_snk_sendr_resp_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_sendr_resp_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_sink_wait_cap_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_sink_wait_cap_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_sink_wait_cap_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_sink_wait_cap_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_sink_act_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_sink_act_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_sink_act_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_sink_act_tmr!\n");
			return false;
		}
	}
	return true;
}

bool sii_snk_dr_swap_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)
	    pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr = (struct sii_usbpd_protocol *)
	    drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("DR SWAP POLICY ENGINE\n");

	pr_info("DR Swap state:%d\n", pdev->dr_swap_state);
	pdev->pr_swap.done = false;

	switch (pdev->dr_swap_state) {
	case PE_SNK_DR_Swap_Init:
		if (pdev->drv_context->pUsbpd_dp_mngr->dr_swap_supp) {
			pdev->dr_swap.done = false;
			pdev->dr_swap_state = PE_DRS_UFP_DFP_Send_DR_Swap;
			work = 1;
		}
		break;
	case PE_DRS_UFP_DFP_Send_DR_Swap:
		pr_info("SEND_COMMAND: DR_SWAP\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__DR_SWAP);
		pdev->dr_swap_state = PE_SWAP_Wait_Good_Crc_Received;
		break;
	case PE_SWAP_Wait_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->dr_swap_state = PE_PRS_Wait_Accept_DR_Swap;
			sii_timer_start(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs))
				work = 1;
			else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs))
				work = 1;
			else if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs))
				work = 1;
		}
		break;
	case PE_PRS_Wait_Accept_DR_Swap:
		if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap_state = PE_DRS_UFP_DFP_Change_to_DFP;
			work = 1;
		} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			clear_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
			pdev->dr_swap_state = PE_SNK_DR_Swap_Init;
			pr_info("DR SWAP REJECTED\n");
		} else if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
			pr_info("PEER IS NOT READY FOR DR_SWAP\n");
			if (pdev->custom_msg) {
				pr_info("CUSTOM MESSAGE NOT SUPPORTED(MMI)\n");
				pdev->custom_msg = 0;
			}
		}
		break;

	case PE_DRS_UFP_DFP_Change_to_DFP:
		change_drp_data_role(pdev);
		pr_info("DR SWAP COMPLETED\n");
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(drv_context, PD_DR_SWAP_DONE, 0x00, NULL);
#endif
		pdev->dr_swap.req_send = false;
		pdev->dr_swap_state = PE_SNK_DR_Swap_Init;
		pdev->dr_swap.done = true;
		pdev->dr_swap.req_rcv = false;
		si_update_pd_status(pdev);
		if (pdev->custom_msg)
			work = 1;
		break;

	case PE_DRS_UFP_DFP_Evaluate_DR_Swap:
		if (pdev->drv_context->pUsbpd_dp_mngr->dr_swap_supp) {
			pdev->dr_swap_state = PE_DRS_UFP_DFP_Accept_DR_Swap;
			work = 1;
		} else {
			pdev->dr_swap_state = PE_DRS_UFP_DFP_Reject_DR_Swap;
			work = 1;
		}
		break;

	case PE_DRS_UFP_DFP_Accept_DR_Swap:
		pr_info("SEND_COMMAND: ACCEPT\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__ACCEPT);
		pdev->dr_swap_state = PE_Wait_Accept_Good_Crc_Received;
		break;

	case PE_Wait_Accept_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->dr_swap_state = PE_DRS_UFP_DFP_Change_to_DFP;
			work = 1;
		} else {
		}
		break;
	case PE_DRS_Swap_exit:
		pdev->dr_swap.req_send = false;
		pdev->dr_swap.req_rcv = false;
		pdev->dr_swap.done = false;
		break;
	case PE_DRS_UFP_DFP_Reject_DR_Swap:
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__REJECT);
		pdev->dr_swap_state = PE_SNK_Wait_Reject_Good_Crc_Received;
		break;
	case PE_SNK_Wait_Reject_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pr_info("DR SWAP NOT SUPPORTED\n");
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.in_progress = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
		}
		break;
	default:
		break;
	}
	return work;
}

bool sii_snk_pr_swap_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_typec *ptypec_dev = (struct sii_typec *)drv_context->ptypec;
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("SWAP POLICY ENGINE\n");

	pr_info("SWap state:%d\n", pdev->pr_swap_state);
	switch (pdev->pr_swap_state) {
	case PE_SNK_Swap_Init:
		pdev->pr_swap_state = PE_PRS_SNK_SRC_Send_Swap;
		work = 1;
		pdev->hard_reset_counter = 0;
		break;
	case PE_PRS_SNK_SRC_Send_Swap:
		pr_info("SEND_COMMAND: SEND_SWAP\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PR_SWAP);
		pdev->pr_swap_state = PE_SWAP_Wait_Good_Crc_Received;
		pdev->pr_swap.in_progress = true;/*done here only for nuvuton*/
		break;
	case PE_SWAP_Wait_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap.in_progress = true;
			pdev->pr_swap_state = PE_PRS_Wait_Accept_PR_Swap;
			sii_timer_start(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs))
				work = 1;
			else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs))
				work = 1;
		} else {
			pdev->pr_swap.in_progress = false;
		}
		break;
	case PE_PRS_Wait_Accept_PR_Swap:
		if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->pr_swap_state = PE_PRS_SNK_SRC_Transition_to_off;
			work = 1;
			pdev->pr_swap.in_progress = true;
		} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
			clear_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pr_info("PR SWAP REJECTED FROM PEER\n");
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = false;
			pdev->pr_swap_state = PE_SNK_Swap_Init;
		}

		break;
	case PE_PRS_SNK_SRC_Transition_to_off:
		pr_debug("PE_PRS_SNK_SRC_Transition_to_off\n");

		sii_timer_start(&(pdev->usbpd_ps_source_off_tmr));
		pdev->pr_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
		if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs))
			work = 1;
		break;
	case PE_SWAP_Wait_Ps_Rdy_Received:
		pr_debug("PE_SWAP_Wait_Ps_Rdy_Received\n");
		if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_ps_source_off_tmr));
			clear_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs);
			if (!ptypec_dev->typecDrp)
				pdev->pr_swap_state = PE_PRS_SNK_SRC_Source_on;
			else
				pdev->pr_swap_state = PE_PRS_SNK_SRC_Assert_Rp;

			work = 1;
			/*sii70xx_vbus_enable(pdev->drv_context,
			   VBUS_DEFAULT); */
		}
		break;
	case PE_PRS_SNK_SRC_Source_on:
		change_drp_pwr_role(pdev);
		if (test_bit(PING_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			clear_bit(PING_RCVD, &pdev->intf.param.sm_cmd_inputs);
		} else {
			pr_info("SEND_COMMAND: PS_RDY\n");
			usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PS_RDY);
			pdev->pr_swap_state = PE_SWAP_PS_RDY_Ack_Rcvd;
		}
		break;
	case PE_SWAP_PS_RDY_Ack_Rcvd:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = true;
			pr_info("PR SWAP COMPLETED\n");
			si_update_pd_status(pdev);
			set_70xx_mode(pdev->drv_context, TYPEC_DRP_DFP);
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_PR_SWAP_DONE, 0x00, NULL);
#endif
		}
		break;
	case PE_PRS_SNK_SRC_Evaluate_Swap:
		pr_debug("PE_PRS_SNK_SRC_Evaluate_Swap\n");
		if (pdev->drv_context->pUsbpd_dp_mngr->pr_swap_supp)
			pdev->pr_swap_state = PE_PRS_SNK_SRC_Accept_Swap;
		else
			pdev->pr_swap_state = PE_PRS_SNK_SRC_Reject_Swap;
		work = 1;
		break;
	case PE_PRS_SNK_SRC_Accept_Swap:
		pr_debug("PE_PRS_SNK_SRC_Accept_Swap\n");
		pr_info("SEND_COMMAND: ACCEPT\n");
		pdev->pr_swap.in_progress = true;
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__ACCEPT);
		pdev->pr_swap_state = PE_SWAP_Wait_Accept_Good_CRC_Received;
		break;
	case PE_SWAP_Wait_Accept_Good_CRC_Received:
		if (pdev->tx_good_crc_received) {
			pdev->pr_swap.in_progress = true;
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap_state = PE_PRS_SNK_SRC_Transition_to_off;
			work = 1;
		}
		break;
	case PE_PRS_SNK_SRC_Assert_Rp:
		pr_debug("PE_PRS_SNK_SRC_Assert_Rp\n");
		pdev->pr_swap_state = PE_PRS_SNK_SRC_Source_on;
		work = 1;

		break;
	case PE_PRS_Swap_exit:
		pdev->pr_swap.req_send = false;
		pdev->pr_swap.in_progress = false;
		pdev->pr_swap.req_rcv = false;
		pdev->pr_swap.done = false;
		pr_info("PR SWAP EXIT\n");
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(drv_context, PD_PR_SWAP_EXIT, 0x00, NULL);
#endif
		break;
	case PE_PRS_SNK_SRC_Reject_Swap:
		pr_info("SEND_COMMAND: REJECT\n");

		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__REJECT);
		pdev->pr_swap_state = PE_SRC_Wait_Reject_Good_Crc_Received;
		break;
	case PE_SNK_Wait_Reject_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pr_info("PR SWAP NOT SUPPORTED\n");
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = false;
		}
		break;

	default:
		break;
	}
	return work;
}

bool sii_snk_vdm_mode_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	/*struct sii_typec *ptypec_dev =
	   (struct sii_typec *)drv_context->ptypec; */
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("ALT MODE POLICY ENGINE\n");

	clear_bit(VDM_MSG_RCVD, &pdev->intf.param.svdm_sm_inputs);

	if (test_bit(SVDM_DISC_IDEN_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
		clear_bit(SVDM_DISC_IDEN_RCVD, &pdev->intf.param.svdm_sm_inputs);
		pdev->alt_mode_state = PE_UFP_VDM_Get_Identity;
	}
	if (test_bit(DISCOVER_SVID_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
		clear_bit(DISCOVER_SVID_RCVD, &pdev->intf.param.svdm_sm_inputs);
		pdev->alt_mode_state = PE_UFP_VDM_Get_SVIDs;
	}
	if (test_bit(DISCOVER_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
		clear_bit(DISCOVER_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs);
		pdev->alt_mode_state = PE_UFP_VDM_Get_Modes;
	}
	if (test_bit(ENTER_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
		clear_bit(ENTER_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs);
		pdev->alt_mode_state = PE_UFP_VDM_Evaluate_Mode_Entry;
	}
	if (test_bit(EXIT_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
		clear_bit(EXIT_MODE_RCVD, &pdev->intf.param.svdm_sm_inputs);
		pdev->alt_mode_state = PE_UFP_VDM_Mode_Exit_ACK;
	}

	pr_info("Alt mode state:%d\n", pdev->alt_mode_state);
	switch (pdev->alt_mode_state) {
	default:
		pdev->alt_mode_cmnd_xmit = false;
		break;
	case PE_UFP_VDM_Get_Identity:
		if (pdev->drv_context->pUsbpd_dp_mngr->identity_request) {
			pdev->alt_mode_state = PE_UFP_VDM_Send_Identity;
			work = 1;
			pdev->alt_mode_cmnd_xmit = true;
		}
		break;
	case PE_UFP_VDM_Send_Identity:
		pr_debug("PE_UFP_VDM_Send_Identity\n");
		usbpd_svdm_init_resp(pUsbpd_prtlyr, CMD_DISCOVER_IDENT, false, NULL);
		pdev->alt_mode_state = PE_VDM_Wait_Good_Src_Received;
		break;
	case PE_VDM_Wait_Good_Src_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			work = 0;
			pdev->alt_mode_cmnd_xmit = false;
		}
		break;
	case PE_UFP_VDM_Get_SVIDs:
		if (pdev->drv_context->pUsbpd_dp_mngr->svid_request) {
			pdev->alt_mode_state = PE_UFP_VDM_Send_SVIDs;
			work = 1;
			pdev->alt_mode_cmnd_xmit = true;
		}
		break;
	case PE_UFP_VDM_Send_SVIDs:
		pr_debug("PE_UFP_VDM_Send_SVIDs\n");
		usbpd_svdm_init_resp(pUsbpd_prtlyr, CMD_DISCOVER_SVID, false, NULL);
		pdev->alt_mode_state = PE_VDM_SVID_Wait_Good_Src_Received;
		break;
	case PE_VDM_SVID_Wait_Good_Src_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			work = 0;
			pdev->alt_mode_cmnd_xmit = false;
		}
		break;
	case PE_UFP_VDM_Get_Modes:
		if (pdev->drv_context->pUsbpd_dp_mngr->modes_request) {
			pdev->alt_mode_state = PE_UFP_VDM_Send_Modes;
			work = 1;
			pdev->alt_mode_cmnd_xmit = true;
		}
		break;
	case PE_UFP_VDM_Send_Modes:
		pr_debug("PE_UFP_VDM_Send_SVIDs\n");
		usbpd_svdm_init_resp(pUsbpd_prtlyr, CMD_DISCOVER_MODES, false, NULL);
		pdev->alt_mode_state = PE_VDM_Modes_Wait_Good_Src_Received;
		break;
	case PE_VDM_Modes_Wait_Good_Src_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			work = 0;
			pdev->alt_mode_cmnd_xmit = false;
		}
		break;
	case PE_UFP_VDM_Evaluate_Mode_Entry:
		if (pdev->drv_context->pUsbpd_dp_mngr->entry_request) {
			pdev->alt_mode_state = PE_UFP_VDM_Mode_Entry_ACK;
			work = 1;
			pdev->alt_mode_cmnd_xmit = true;
		}
		break;
	case PE_UFP_VDM_Mode_Entry_ACK:
		pr_info("PE_UFP_VDM_Mode_Entry_ACK\n");
		usbpd_svdm_init_resp(pUsbpd_prtlyr, CMD_ENTER_MODE, false, NULL);
		pdev->alt_mode_state = PE_VDM_Enter_Wait_Good_Src_Received;
		break;
	case PE_VDM_Enter_Wait_Good_Src_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			work = 0;
			pdev->alt_mode_cmnd_xmit = false;
			/*si_enable_switch_control(pdev->drv_context,
			   PD_RX, true); */
			pr_info("ALt mode completed\n");
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_UFP_ENTER_MODE_DONE, 0x00, NULL);
#endif
#if defined(SII_LINUX_BUILD)
#else
			pdev->pr_swap.req_send = true;
			work = true;
#endif
		}
		break;
	case PE_UFP_VDM_Mode_Exit_ACK:
		pr_info("PE_UFP_VDM_Mode_Exit_ACK\n");
		usbpd_svdm_init_resp(pUsbpd_prtlyr, CMD_EXIT_MODE, false, NULL);
		pdev->alt_mode_cmnd_xmit = true;
		pdev->alt_mode_state = PE_Exit_Mode_Good_Crc_Received;
		break;
	case PE_Exit_Mode_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			work = 0;
			pdev->alt_mode_cmnd_xmit = false;
			/*si_enable_switch_control(pdev->drv_context,
			   PD_RX, false); */
			pr_info("ALT MODE DISCONNECTED\n");
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_UFP_EXIT_MODE_DONE, 0x00, NULL);
#endif
		}
		break;
	}
	return work;
}

bool sii_snk_vconn_swap_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("VCONN POLICY ENGINE\n");

	pr_info("vconn state:%d\n", pdev->vconn_swap_state);
	pdev->dr_swap.done = false;
	switch (pdev->vconn_swap_state) {
	case PE_VCS_UFP_Evaluate_Swap:
		if (pdev->drv_context->pUsbpd_dp_mngr->vconn_swap) {
			pdev->vconn_swap_state = PE_VCS_UFP_Accept_Swap;
			work = 1;
		}
		break;

	case PE_VCS_UFP_Accept_Swap:
		pr_info("SEND_COMMAND: ACCEPT\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__ACCEPT);
		pdev->vconn_swap_state = PE_SNK_Wait_Accept_Good_Crc_Received;
		break;
	case PE_SNK_Wait_Accept_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			if (pdev->vconn_is_on)
				pdev->vconn_swap_state = PE_VCS_UFP_Wait_for_DFP_VCONN;
			else
				pdev->vconn_swap_state = PE_VCS_UFP_Turn_On_VCONN;
			work = 1;
		}
		break;
	case PE_VCS_UFP_Wait_for_DFP_VCONN:
		sii_timer_start(&(pdev->usbpd_inst_vconn_on_tmr));
		pdev->vconn_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
		break;

	case PE_SWAP_Wait_Ps_Rdy_Received:
		pr_info("PE_VCONN_Wait_Ps_Rdy_Received\n");
		if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			clear_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs);
			sii_timer_stop(&(pdev->usbpd_inst_vconn_on_tmr));
			pdev->vconn_swap_state = PE_VCS_UFP_Turn_Off_VONN;
			work = 1;
		}
		break;
	case PE_VCS_UFP_Turn_Off_VONN:
		pdev->vconn_swap.done = true;
		pdev->vconn_swap.req_send = false;
		pdev->vconn_swap.enable = false;
		pdev->vconn_swap.req_rcv = false;
		/*si_update_pd_status(pdev); */
		work = 1;
		pr_info("VCONN_COMPLETED\n");
		break;
	case PE_VCS_UFP_Turn_On_VCONN:
		pdev->vconn_swap_state = PE_VCS_DFP_Send_PS_Rdy;
		work = 1;
		pdev->vconn_swap.enable = true;
		/*si_update_pd_status(pdev); */
		break;
	case PE_VCS_DFP_Send_PS_Rdy:
		pr_info("SEND_COMMAND: PS_RDY\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PS_RDY);
		pdev->vconn_swap_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
		break;
	case PE_SRC_Wait_Src_rdy_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
			pdev->vconn_swap.done = true;
			pdev->vconn_swap.req_send = false;
			pdev->vconn_swap.req_rcv = false;
			pr_info("VCONN_COMPLETED\n");
		}
		break;
	default:
		break;
	}
	return work;

}

void sink_policy_engine(WORK_STRUCT *w)
{
	struct sii_usbp_policy_engine *pdev = container_of(w, struct sii_usbp_policy_engine,
							   pd_ufp_work);
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_protlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;
	struct sii_typec *ptypec_dev = (struct sii_typec *)
	    drv_context->ptypec;

	uint8_t work = 0;

	int result = 0;

	pr_info("SINK POLICY ENGINE\n");
	if (!pdev) {
		pr_err("Not able to find address\n");
		return;
	}
	if (!pdev->pd_connected) {
		pr_info("DFP not Connected\n");
		return;
	}
	pr_info("SINK LOCK WAIT\n");
	if (!down_interruptible(&drv_context->isr_lock)) {
		pr_debug("SINK STATE - %d %d %lx\n",
			 pdev->state, pdev->pr_swap_state, pdev->intf.param.sm_cmd_inputs);
		switch (pdev->state) {
		case PE_SNK_Startup:
			pr_info("PE_SNK_Startup\n");

			/*      result = usbpd_core_reset(pdev->drv_context); */
			work = 1;
			pdev->next_state = PE_SNK_Discovery;
			pdev->custom_msg = 0;
			break;

		case PE_SNK_Discovery:
			if (ptypec_dev->is_vbus_detected) {
				work = 1;
				pdev->next_state = PE_SNK_Wait_for_Capabilities;
			} else {
				pr_debug("Vbus not detected\n");
			}
			break;

		case PE_SNK_Wait_for_Capabilities:
			sii_timer_start(&(pdev->usbpd_inst_sink_wait_cap_tmr));
			if (test_bit(SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				sii_timer_stop(&pdev->usbpd_inst_sink_wait_cap_tmr);
				pdev->next_state = PE_SNK_Evaluate_Capability;
				work = 1;
			} else {
				pr_info("No caps received from the DFP\n");
			}
			break;

		case PE_SNK_Evaluate_Capability:
			sii_timer_stop(&pdev->usbpd_inst_no_resp_tmr);
			pdev->hard_reset_counter = 0;
			work = 1;
			pdev->next_state = PE_SNK_Select_Capability;
			break;

		case PE_SNK_Select_Capability:
			send_request_msg(pdev);
			pr_info("SEND_COMMAND: REQUEST\n");

			sii_usbpd_xmit_data_msg(pUsbpd_protlyr, REQ, &pdev->rdo, 1);
			pdev->next_state = PE_SNK_Wait_Request_Good_Crc_Received;
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_POWER_LEVELS, 0x00, NULL);
#endif
			break;
		case PE_SNK_Wait_Accept_Sft_Rst_Received:
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->next_state = PE_SNK_Startup;
			} else {
				pdev->next_state = PE_SNK_Hard_Reset;
				work = 1;
			}
			break;
		case PE_SNK_Wait_Request_Good_Crc_Received:
			if (pdev->tx_good_crc_received) {
				sii_timer_start(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
				if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs))
					work = 1;
				pdev->next_state = PE_SNK_Transition_Sink;
				pdev->tx_good_crc_received = 0;
			} else {
				pr_debug("GOOD CRC Is not received\n");
				pr_info("SEND_COMMAND: SOFT_RESET\n");
				usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__SOFT_RESET);
				pdev->busy_flag = 0;
				pdev->next_state = PE_SNK_Wait_sft_rst_Good_Crc_Received;
			}
			break;

		case PE_SNK_Wait_sft_rst_Good_Crc_Received:
			pr_debug("PE_SFTWaitSrc_rdy_good_Crc_Received\n");
			if (pdev->tx_good_crc_received) {
				pdev->next_state = PE_SNK_Wait_Accept_Sft_Rst_Received;
				pr_info("soft reset done\n");
				pdev->tx_good_crc_received = 0;
				work = 1;
			} else {
				pr_debug("\nGOOD CRC not received - 3\n");
				pdev->next_state = PE_SNK_Hard_Reset;
				work = 1;
			}
			break;

		case PE_SNK_Transition_Sink:
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
				clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
				sii_timer_start(&(pdev->usbpd_inst_ps_trns_tmr));

				/*above line SNK_CHANGE */
				pdev->next_state = PE_SNK_Wait_Ps_Rdy_Received;
				break;
			} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs);
				sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
				pdev->next_state = PE_SNK_Ready;
				work = 1;
			} else if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				sii_timer_stop(&(pdev->usbpd_inst_snk_sendr_resp_tmr));
				pdev->next_state = PE_SNK_Ready;
				work = 1;
			} else {
				pr_debug("No Command Received\n");
			}
			break;

		case PE_SNK_Wait_Ps_Rdy_Received:
			if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs);
				sii_timer_stop(&(pdev->usbpd_inst_ps_trns_tmr));
				pdev->src_cap_req = 0;
				work = 1;
				pr_info("%s:POWER CONTRACT ESTABLISHED:%s\n",
					ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
				pdev->next_state = PE_SNK_Send_Goto_Min;
			}
			break;
		case PE_SNK_Send_Goto_Min:
			usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__GO_TO_MIN);
			pdev->next_state = PE_SNK_Wait_Good_Crc_Goto_Min;
			break;

		case PE_SNK_Wait_Good_Crc_Goto_Min:
			pr_debug("PE_SNK_Wait_Good_Crc_Goto_Min\n");
			if (pdev->tx_good_crc_received) {
				pdev->next_state = PE_SNK_Ready;
				pdev->tx_good_crc_received = 0;
				work = 1;
			}
			break;

		case PE_SNK_Ready:
			pr_info("!!!! ...SINK READY... !!!!\n");
			if (test_bit(SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->next_state = PE_SNK_Evaluate_Capability;
				work = 1;
				break;
			}
			if (pdev->dr_swap.done) {
				if (pdev->custom_msg) {
					up(&drv_context->isr_lock);
					si_custom_msg_xmit(pdev);
					pdev->next_state = PE_SNK_Ready;
					work = 1;
					wakeup_dfp_queue(drv_context);
					return;
				}
				pdev->dr_swap.done = false;
			}
			if (test_bit(SOFT_RESET_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(SOFT_RESET_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pr_info("SEND_COMMAND: ACCEPT\n");
				usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__ACCEPT);
				break;
			}
			if ((test_bit(VDM_MSG_RCVD, &pdev->intf.param.svdm_sm_inputs)) ||
			    (pdev->alt_mode_cmnd_xmit)) {
				work = sii_snk_vdm_mode_engine(pdev);
				break;
			}
			/****************************************************/
			if (test_bit(VCONN_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(VCONN_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->vconn_swap.req_rcv = 1;
				pdev->vconn_swap_state = PE_VCS_UFP_Evaluate_Swap;
			}
			if (pdev->vconn_swap.req_rcv == 1) {
				pdev->next_state = PE_SNK_Ready;
				if (sii_snk_vconn_swap_engine(pdev))
					work = 1;
				/*disable altmode commands */
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			/***************************PR_SWAP******************/
			if (pdev->pr_swap.req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SNK_Ready;
				if (sii_snk_pr_swap_engine(pdev))
					work = 1;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(PR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(PR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->pr_swap.req_rcv = 1;
				pdev->pr_swap_state = PE_PRS_SNK_SRC_Evaluate_Swap;
			}
			if (pdev->pr_swap.req_rcv == 1) {
				pdev->next_state = PE_SNK_Ready;
				if (sii_snk_pr_swap_engine(pdev))
					work = 1;
				/*disable altmode commands */
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			/***************************DR_SWAP********************/
			if (pdev->dr_swap.req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SNK_Ready;
				if (sii_snk_dr_swap_engine(pdev))
					work = 1;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(DR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(DR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->dr_swap_state = PE_DRS_UFP_DFP_Evaluate_DR_Swap;
				work = 1;
				pdev->dr_swap.req_rcv = 1;
				pdev->alt_mode_cmnd_xmit = false;
			}
			if (pdev->dr_swap.req_rcv == 1) {
				pdev->next_state = PE_SNK_Ready;
				if (sii_snk_dr_swap_engine(pdev))
					work = 1;
				break;
			}
			/*****************************************************/
			if (test_bit(GOTOMIN_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(GOTOMIN_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SNK_Transition_Sink;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				sii_timer_start(&(pdev->usbpd_inst_sink_req_tmr));
				clear_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SNK_Ready;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(GET_SINK_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(GET_SINK_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SNK_Give_Sink_Cap;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(GET_SINK_SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(GET_SINK_SRC_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SNK_Get_Source_Cap;
				pdev->alt_mode_cmnd_xmit = false;
				break;
			}
			if (test_bit(GET_SOURCE_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)
			    || (pdev->src_cap_req)) {
				/*pr_debug("get source cap received\n"); */
				clear_bit(GET_SOURCE_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SNK_Get_Source_Cap;
				break;
			}
			break;

		case PE_SNK_Give_Sink_Cap:
			pr_debug("PE_SNK_Give_Sink_Cap\n");
			sii_usbpd_get_snk_cap(pdev, pUsbpd_protlyr->send_msg, FIXED_SUPPLY);
			pr_info("SEND_COMMAND: SNK_CAP\n");
			sii_usbpd_xmit_data_msg(pUsbpd_protlyr,
						SNKCAP, pUsbpd_protlyr->send_msg, 1);
			pdev->next_state = PE_SNK_Wait_Accept_Good_Crc_Received;
			break;

		case PE_SNK_Wait_Accept_Good_Crc_Received:
			if (pdev->tx_good_crc_received) {
				work = 1;
				pdev->next_state = PE_SNK_Ready;
				pdev->tx_good_crc_received = false;
				pdev->src_cap_req = false;
			}
			break;
		case PE_SNK_Get_Source_Cap:
			pr_debug("PE_SNK_Get_Source_Cap\n");
			pr_info("SEND_COMMAND: GET_SOURCE_CAP\n");
			usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__GET_SRC_CAP);

			pdev->next_state = PE_SNK_Wait_Accept_Good_Crc_Received;
			break;
		case PE_SNK_Transition_to_default:
			pr_debug("PE_SNK_Transition_to_default\n");
			sii_timer_start(&(pdev->usbpd_inst_no_resp_tmr));
			pdev->next_state = PE_SNK_Startup;
			work = 1;
			break;
		case PE_SNK_Hard_Reset:
			pr_debug("PE_SNK_HARD_RESET\n");
			result = send_hardreset(pdev);

			if (result) {
				pdev->hard_reset_counter++;
				pdev->next_state = PE_SNK_Wait_HR_Good_Crc_Received;
			} else
				pdev->next_state = ErrorRecovery;
			break;
		case PE_SNK_Wait_HR_Good_Crc_Received:
			sii_platform_set_bit8(REG_ADDR__PDCTR0,
					      BIT_MSK__PDCTR0__RI_PRL_HCRESET_DONE_BY_PE_WP);
			pdev->next_state = PE_SNK_Transition_to_default;
			work = 1;
			break;
		case ErrorRecovery:
			pr_debug("Error Recovery....\n");
			break;
		default:
			break;
		}
		pdev->state = pdev->next_state;
		up(&drv_context->isr_lock);
	}
	if (work) {
		work = 0;
		wakeup_ufp_queue(drv_context);
	}
	pr_debug("UFP QUEUE PROCESSED\n");
}

void wakeup_ufp_queue(struct sii70xx_drv_context *drv_context)
{
	struct sii_usbp_policy_engine *pUsbpd =
	    (struct sii_usbp_policy_engine *)drv_context->pusbpd_policy;

	if (pUsbpd->pd_connected)
		sii_wakeup_queues(pUsbpd->ufp_work_queue,
				  &pUsbpd->pd_ufp_work, &pUsbpd->is_event, true);
	else
		pr_info("\ncable is disconnected\n");
}

void sii_reset_ufp(struct sii_usbp_policy_engine *pUsbpd)
{
	if (pUsbpd->usbpd_inst_sink_act_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_sink_act_tmr);
	if (pUsbpd->usbpd_inst_sink_wait_cap_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_sink_wait_cap_tmr);
	if (pUsbpd->usbpd_inst_snk_sendr_resp_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_snk_sendr_resp_tmr);
	if (pUsbpd->usbpd_inst_ps_trns_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_ps_trns_tmr);
	if (pUsbpd->usbpd_ps_source_off_tmr)
		sii_timer_stop(&pUsbpd->usbpd_ps_source_off_tmr);

	pUsbpd->state = PE_SNK_Startup;
	pUsbpd->pr_swap_state = PE_SNK_Swap_Init;
	pUsbpd->dr_swap_state = PE_SNK_DR_Swap_Init;
	pUsbpd->alt_mode_state = PE_UFP_VDM_Get_Identity;

	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));

	pUsbpd->alt_mode_req_rcv = false;
	pUsbpd->alt_mode_req_send = false;
	pUsbpd->exit_mode_req_send = false;
	pUsbpd->custom_msg = false;
	pUsbpd->vdm_cnt = 0;
	pUsbpd->src_cap_req = false;
	pUsbpd->alt_mode_cmnd_xmit = false;
	if (pUsbpd->at_mode_established == true) {
		pUsbpd->at_mode_established = false;
		/*si_enable_switch_control(pUsbpd->drv_context, PD_RX, false); */
	}
}

bool usbpd_set_ufp_swap_init(struct sii_usbp_policy_engine *pUsbpd)
{
	bool ret = true;

	if (!pUsbpd) {
		pr_debug("%s: Error initialisation.\n", __func__);
		return false;
	}
	pr_info("\nusbpd_set_ufp_swap_init:%x\n", pUsbpd->state);

	sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK28 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK29);

	sii_platform_set_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK25);

	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_DATA_ROLE);
	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_POWER_ROLE);

	pUsbpd->pd_connected = true;
	pUsbpd->alt_mode_cmnd_xmit = false;

	sii_platform_clr_bit8(REG_ADDR__PDCTR11,
			      BIT_MSK__PDCTR11__RI_PRL_RX_DISABLE_GOODCRC_DISCARD);

	set_pd_reset(pUsbpd->drv_context, false);

	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->vconn_swap, 0, sizeof(struct swap_config));

	pUsbpd->intf.param.sm_cmd_inputs = 0;
	pUsbpd->intf.param.svdm_sm_inputs = 0;
	pUsbpd->intf.param.uvdm_sm_inputs = 0;
	pUsbpd->intf.param.count = 0;

	pUsbpd->state = PE_SNK_Startup;
	pUsbpd->next_state = PE_SNK_Startup;
	pUsbpd->pr_swap_state = PE_SNK_Swap_Init;
	pUsbpd->alt_mode_state = PE_UFP_VDM_Get_Identity;

	if (!usbpd_sink_timer_create(pUsbpd)) {
		pr_err("%s: Failed in timer create.\n", __func__);
		ret = false;
		goto exit;
	}
	pUsbpd->drv_context->drp_config = USBPD_UFP;
	pr_info("\nusbpd_set_ufp_swap_init:%x\n", pUsbpd->state);
	wakeup_ufp_queue(pUsbpd->drv_context);
exit:	return ret;
}


bool usbpd_set_ufp_init(struct sii_usbp_policy_engine *pUsbpd)
{
	bool ret = true;

	if (!pUsbpd) {
		pr_debug("%s: Error initialisation.\n", __func__);
		return false;
	}
	pr_info("\nusbpd_set_ufp_init:%x\n", pUsbpd->state);
	pUsbpd->drv_context->drp_config = USBPD_UFP;
	pUsbpd->drv_context->old_drp_config = USBPD_UFP;


	update_data_role(pUsbpd->drv_context, USBPD_UFP);
	update_pwr_role(pUsbpd->drv_context, USBPD_UFP);

	pUsbpd->pd_connected = true;
	pUsbpd->alt_mode_cmnd_xmit = false;

	sii_platform_clr_bit8(REG_ADDR__PDCTR11,
			      BIT_MSK__PDCTR11__RI_PRL_RX_DISABLE_GOODCRC_DISCARD);

	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_DATA_ROLE);
	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_POWER_ROLE);

	set_pd_reset(pUsbpd->drv_context, false);

	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->vconn_swap, 0, sizeof(struct swap_config));

	pUsbpd->state = PE_SNK_Startup;
	pUsbpd->next_state = PE_SNK_Startup;
	pUsbpd->pr_swap_state = PE_SNK_Swap_Init;
	pUsbpd->at_mode_established = false;
	pUsbpd->vdm_cnt = 0;
	pUsbpd->custom_msg = false;

	sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK28 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK29);
	sii_platform_set_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK25);

	if (!usbpd_sink_timer_create(pUsbpd)) {
		pr_debug("%s: Failed in timer create.\n", __func__);
		ret = false;
		goto exit;
	}
	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_RX_SKIP_GOODCRC_RDBUF);
	si_enable_switch_control(pUsbpd->drv_context, PD_RX, true);
	wakeup_ufp_queue(pUsbpd->drv_context);
	pr_info("UFP Init done:%x\n", pUsbpd->state);

exit:	return ret;
}

void usbpd_ufp_exit(struct sii_usbp_policy_engine *pUsbpd)
{
	usbpd_sink_timer_delete(pUsbpd);
	pUsbpd->custom_msg = false;
}
