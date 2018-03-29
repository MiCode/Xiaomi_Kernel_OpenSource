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

#define DFP 1

/**********************************************************************/
int sii_usbpd_src_exit_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected)
		return -ENODEV;

	if (!pUsbpd->at_mode_established) {
		pr_info("ALT Mode is not established\n");
		return -EINVAL;
	}
	if (pUsbpd->exit_mode_req_send) {
		pr_info("Exit mode in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SRC_Ready) {
		pr_info("Exit mode initiated\n");
		pUsbpd->exit_mode_req_send = true;
		pUsbpd->alt_mode_state = PE_DFP_VDM_Mode_Exit_Request;
		wakeup_dfp_queue(pUsbpd->drv_context);
		return true;
	}
	pr_info("PD is not established\n");
	return -EINVAL;
}

int sii_usbpd_src_alt_mode_req(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("PD is not Established\n");
		return -ENODEV;
	}

	if (pUsbpd->at_mode_established || pUsbpd->alt_mode_req_send) {
		pr_info("Already in MHL Mode\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SRC_Ready) {
		pUsbpd->alt_mode_req_rcv = false;
		pUsbpd->alt_mode_req_send = true;
		pUsbpd->alt_mode_state = PE_DFP_VDM_Init;
		wakeup_dfp_queue(pUsbpd->drv_context);
		pr_info("Alt mode initiated\n");
		return true;
	}
	pr_info("PD is not established\n");
	return -EINVAL;
}

int sii_usbpd_src_pwr_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("PD is not Established\n");
		return -ENODEV;
	}

	if (pUsbpd->pr_swap.req_send || pUsbpd->pr_swap.req_rcv) {
		pr_info("pr_swap in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SRC_Ready) {
		pUsbpd->pr_swap.req_send = true;
		pUsbpd->pr_swap_state = PE_SRC_Swap_Init;
		wakeup_dfp_queue(pUsbpd->drv_context);
		pr_info("pr_swap initiated\n");
		return true;
	} else {
		return -EINVAL;
	}

}

int sii_usbpd_src_data_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("PD is not Established\n");
		return -ENODEV;
	}

	if (pUsbpd->dr_swap.req_send) {
		pr_info("dr_swap in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SRC_Ready) {
		pUsbpd->dr_swap.req_send = true;
		pUsbpd->dr_swap_state = PE_SRC_DR_Swap_Init;
		wakeup_dfp_queue(pUsbpd->drv_context);
		pr_info("dr_swap initiated\n");
		return true;
	} else {
		return -EINVAL;
	}

}

int sii_usbpd_req_src_vconn_swap(struct sii_usbp_policy_engine *pUsbpd, uint8_t portnum)
{
	if (!pUsbpd->pd_connected) {
		pr_info("PD is not Established\n");
		return -ENODEV;
	}

	if (pUsbpd->src_vconn_swap_req) {
		pr_info("vconn swap is in progress\n");
		return -EINVAL;
	}
	if (pUsbpd->state == PE_SRC_Ready) {
		pUsbpd->vconn_swap.req_send = true;
		pUsbpd->vconn_swap_state = PE_VCS_DFP_Send_Swap;
		wakeup_dfp_queue(pUsbpd->drv_context);
		pr_info("src_vconn_req initiated\n");
		return true;
	} else {
		return -EINVAL;
	}
}

/**********************************Timers**********************/
static void wait_on_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("wait_on_timer_handler\n");
		wakeup_dfp_queue(pdev->drv_context);
		up(&pdev->drv_context->isr_lock);
	}
}

static void ps_source_on_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("ps_source_on_timer_handler\n");

		if (pdev->pr_swap_state == PE_SWAP_Wait_Ps_Rdy_Received) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SRC_Hard_Reset;
				goto success;
			}
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void sender_response_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("Source Sender Response Timer\n");
		if (pdev->state == PE_SRC_Get_Sink_Cap) {
			pdev->state = PE_SRC_Ready;
			goto success;
		} else if (pdev->state == PE_SRC_Wait_Req_Msg_Received) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SRC_Hard_Reset;
				goto success;
			}
		} else if (pdev->vconn_swap_state == PE_PRS_Wait_Accept_VCONN_Swap) {
			pdev->state = PE_SRC_Ready;
			pdev->vconn_swap.req_send = false;
			goto success;
		} else if (pdev->dr_swap_state == PE_PRS_Wait_Accept_DR_Swap) {
			pdev->dr_swap_state = PE_DRS_Swap_exit;
			goto success;
		} else if (pdev->pr_swap_state == PE_PRS_Wait_Accept_PR_Swap) {
			pdev->pr_swap_state = PE_PRS_Swap_exit;
			goto success;
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void vdm_exit_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_debug("VDM_MODE_EXIT_TIMER\n");

		if (pdev->alt_mode_state == PE_drp_mode_Wait_Exit_ACKed) {
			pdev->alt_mode_state = DFP_SVIDs_EXIT_NAK;
			goto success;
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void source_activity_timer_handler(void *context)
{
	/*struct sii_usbp_policy_engine *pdev =
	   (struct sii_usbp_policy_engine *)context; */
	/*this is optional need to think of this */
	/*pr_info("\n$$ SRC_ACTVTY_TIMER $$\n"); */
}

static void source_capability_timer_handler(void *context)
{

	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("SRC_CAP_TIMER\n");
		if (pdev->state == PE_SRC_Discovery) {
			if (pdev->caps_counter <= N_CAPSCOUNT) {
				pdev->state = PE_SRC_Send_Capabilities;
				goto success;
			} else if (pdev->caps_counter > N_CAPSCOUNT) {
				pdev->state = PE_SRC_Disabled;
				goto success;
			}
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void bist_timer_handler(void *context)
{
	pr_info("BIST_TIMER\n");
	sii_platform_clr_bit8(REG_ADDR__PDCTR18, BIT_MSK__PDCTR18__RI_BIST_ENABLE);
}

static void vdm_mode_entry_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("VDM_MODE_ENTRY_TIMER\n");

		if (pdev->alt_mode_state == DFP_SVIDs_ENTER_MODE_ACK_RCVD) {
			pdev->alt_mode_state = DFP_SVIDs_ENTERY_NAK;
			goto success;
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

static void vdm_response_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("VDM_RESPONSE_TIMER %lx\n", jiffies);
		/*commented because of nuvuton synchronization */
		if (pdev->alt_mode_state == DFP_DISCOVER_SVID_MODES_ACK) {
			pdev->alt_mode_state = DFP_SVIDs_MODES_NAK;
			goto success;
		}
		if (pdev->alt_mode_state == DFP_DISCOVER_SVID_ACK) {
			pdev->alt_mode_state = DFP_SVIDs_SVID_NAK;
			goto success;
		}
		if (pdev->alt_mode_state == PE_DFP_VDM_Wait_Identity_ACK_Rcvd) {
			pdev->alt_mode_state = DFP_SVIDs_IDENTITY_NAK;
			goto success;
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

void bist_timer_start(void *context)
{
	struct sii_usbpd_protocol *pd = (struct sii_usbpd_protocol *)context;
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pd->drv_context;
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)
	    drv_context->pusbpd_policy;

	sii_timer_start(&(pdev->usbpd_inst_tbist_tmr));
}

void vconn_on_timer_handler(void *context)
{
	struct sii_usbp_policy_engine *pdev = (struct sii_usbp_policy_engine *)context;

	if (!down_interruptible(&pdev->drv_context->isr_lock)) {
		pr_info("VCONN_ON_TIMER %lx\n", jiffies);

		if (pdev->vconn_swap_state == PE_SWAP_Wait_Ps_Rdy_Received) {
			if (pdev->hard_reset_counter < N_HARDRESETCOUNT) {
				pdev->state = PE_SRC_Hard_Reset;
				goto success;
			}
		}
		goto exit;
success:	wakeup_dfp_queue(pdev->drv_context);
exit:		up(&pdev->drv_context->isr_lock);
	}
}

bool usbpd_source_timer_create(struct sii_usbp_policy_engine *pUsbpd)
{
	int status = 0;

	if (!pUsbpd) {
		pr_warn("%s: failed in timer init.\n", __func__);
		return false;
	}
	if (!pUsbpd->usbpd_inst_sendr_resp_tmr) {
		status = sii_timer_create(sender_response_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_sendr_resp_tmr,
					  SenderResponseTimer, false);
		if (status != 0) {
			pr_warn("Failed to register SenderResTimer timer!\n");
			return false;
		}
	}
	if (!pUsbpd->usbpd_inst_source_act_tmr) {
		status = sii_timer_create(source_activity_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_source_act_tmr,
					  SourceActivityTimer, false);

		if (status != 0) {
			pr_warn("Failed to register SourceActTimer timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_inst_ps_source_on_tmr) {
		status = sii_timer_create(ps_source_on_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_ps_source_on_tmr,
					  PSSourceOnTimer, false);

		if (status != 0) {
			pr_warn("Failed to register PSSourceOnTimer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_source_cap_tmr) {
		status = sii_timer_create(source_capability_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_source_cap_tmr,
					  SourceCapabilityTimer, false);

		if (status != 0) {
			pr_warn("Failed to register SourceCapabilityTimer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_inst_tbist_tmr) {
		status = sii_timer_create(bist_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_tbist_tmr, tBISTCOUNT, false);

		if (status != 0) {
			pr_warn("Failed to register BISTTimer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_vdm_mode_entry_tmr) {
		status = sii_timer_create(vdm_mode_entry_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_vdm_mode_entry_tmr,
					  VDMModeEntryTimer, false);

		if (status != 0) {
			pr_warn("Failed to register vdm_mode_entry_timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_vdm_resp_tmr) {
		status = sii_timer_create(vdm_response_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_vdm_resp_tmr,
					  VDMResponseTimer, false);

		if (status != 0) {
			pr_warn("Failed to register vdm_response_timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_vdm_exit_tmr) {
		status = sii_timer_create(vdm_exit_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_vdm_exit_tmr,
					  VDMModeExitTimer, false);

		if (status != 0) {
			pr_warn("Failed to register vdm_exit_timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_inst_vconn_on_tmr) {
		status = sii_timer_create(vconn_on_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_inst_vconn_on_tmr,
					  VCONNOnTimer, false);

		if (status != 0) {
			pr_warn("Failed to register vdm_exit_timer!\n");
			goto exit;
		}
	}
	if (!pUsbpd->usbpd_tSnkTransition_tmr) {
		status = sii_timer_create(wait_on_timer_handler,
					  pUsbpd, &pUsbpd->usbpd_tSnkTransition_tmr, 30, false);

		if (status != 0) {
			pr_warn("Failed to register vdm_exit_timer!\n");
			goto exit;
		}
	}

	return true;

exit:	usbpd_source_timer_delete(pUsbpd);
	return false;
}

bool usbpd_source_timer_delete(struct sii_usbp_policy_engine *pUsbpd)
{
	int rc = 0;

	if (pUsbpd->usbpd_tSnkTransition_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_tSnkTransition_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_tSnkTransition_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete vdm_exit_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_vconn_on_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_vconn_on_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_vconn_on_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete usbpd_inst_vconn_on_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_vdm_exit_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_vdm_exit_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_vdm_exit_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete vdm_exit_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_vdm_resp_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_vdm_resp_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_vdm_resp_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete vdm_resp_tmr!\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_vdm_mode_entry_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_vdm_mode_entry_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_vdm_mode_entry_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete vdm_mode_entry_tmr!\n");
			return false;
		}
	}
	if (pUsbpd->usbpd_source_cap_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_source_cap_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_source_cap_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete source_cap_tmr\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_ps_source_on_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_ps_source_on_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_ps_source_on_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete source_cap_tmr\n");
			return false;
		}
	}

	if (pUsbpd->usbpd_inst_source_act_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_source_act_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_source_act_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_source_act_tmr!\n");
			return false;
		}
	}
	if (pUsbpd->usbpd_inst_sendr_resp_tmr) {
		sii_timer_stop(&pUsbpd->usbpd_inst_sendr_resp_tmr);
		rc = sii_timer_delete(&pUsbpd->usbpd_inst_sendr_resp_tmr);
		if (rc != 0) {
			pr_warn("Failed to delete inst_sendr_resp_tmr!\n");
			return false;
		}
	}
	return true;
}

bool sii_src_alt_mode_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	/*struct sii_typec *ptypec_dev =
	   (struct sii_typec *)drv_context->ptypec; */
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("ALT MODE POLICY ENGINE - \t");

	pr_info("Alt mode state:%d\n", pdev->alt_mode_state);

	switch (pdev->alt_mode_state) {

	case PE_DFP_VDM_Init:
		pdev->alt_mode_state = PE_DFP_VDM_Identity_Request;
		work = 1;
		break;

	case PE_DFP_VDM_Identity_Request:
		pr_info("PE_DFP_VDM_Identity_Request\n");
		 /*DPM*/
		pr_info("SEND_COMMAND: DISCOVER_IDENTITY\n");
		usbpd_send_vdm_cmd(pUsbpd_prtlyr, VDM, CMD_DISCOVER_IDENT, pdev->svid_mode);
		pdev->alt_mode_state = PE_VDM_Wait_Good_Src_Received;
		break;
	case PE_VDM_Wait_Good_Src_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->alt_mode_state = PE_DFP_VDM_Wait_Identity_ACK_Rcvd;
			sii_timer_start(&(pdev->usbpd_vdm_resp_tmr));
			if (test_bit(SVDM_DISC_IDEN_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				pr_debug("SVDM_DISC_IDEN_ACK_RCVD\n");
				work = 1;
			}
		}
		break;
	case PE_DFP_VDM_Wait_Identity_ACK_Rcvd:
		if (test_bit(SVDM_DISC_IDEN_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			clear_bit(SVDM_DISC_IDEN_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			pdev->alt_mode_state = PE_DFP_VDM_Identity_ACKed;
			work = 1;
			/*pdev->at_mode_in_progress = true; */
		} else if (test_bit(SVDM_DISC_IDEN_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			clear_bit(SVDM_DISC_IDEN_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			pdev->at_mode_established = false;
			pdev->alt_mode_req_rcv = false;
			pdev->alt_mode_req_send = false;
			pdev->alt_mode_state = PE_DFP_VDM_Init;
			pr_info("NACK RECEIVED\n");
		}
		break;
	case PE_DFP_VDM_Identity_ACKed:
		pr_debug("PE_DFP_VDM_Identity_ACKed\n");
		 /*DPM*/ pdev->alt_mode_state = DFP_Identity_Request;
		work = 1;
		break;
	case DFP_Identity_Request:
		pr_info("SEND_COMMAND: DISCOVER_SVID\n");
		usbpd_send_vdm_cmd(pUsbpd_prtlyr, VDM, CMD_DISCOVER_SVID, pdev->svid_mode);
		pdev->alt_mode_state = DFP_SVIDs_Wait_Good_Crc_Rcvd;
		break;

	case DFP_SVIDs_Wait_Good_Crc_Rcvd:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			sii_timer_start(&(pdev->usbpd_vdm_resp_tmr));
			pdev->alt_mode_state = DFP_DISCOVER_SVID_ACK;
			if (pdev->svid_mode == 0x02) {
				pr_info("DISPLAY PORT INITIATED\n");
				pdev->at_mode_established = false;
				pdev->alt_mode_req_rcv = false;
				pdev->alt_mode_req_send = false;
				pdev->alt_mode_state = PE_DFP_VDM_Init;
				work = 1;
				sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			}
			if (test_bit(DISCOVER_SVID_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				pr_debug("DISCOVER_SVID_ACK_RCVD\n");
				work = 1;
			}
		}
		break;
	case DFP_DISCOVER_SVID_ACK:
		if (test_bit(DISCOVER_SVID_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			clear_bit(DISCOVER_SVID_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			pdev->alt_mode_state = DFP_SVIDs_Request;
			work = 1;
			if (test_bit(DISPLAY_PORT_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				clear_bit(DISPLAY_PORT_RCVD, &pdev->intf.param.svdm_sm_inputs);
				pdev->at_mode_established = false;
				pdev->alt_mode_req_rcv = false;
				pdev->alt_mode_req_send = false;
				pdev->alt_mode_state = PE_DFP_VDM_Init;
				sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
				pr_info("DISPLAY PORT DETECTED\n");
			}
		} else if (test_bit(DISCOVER_SVID_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			clear_bit(DISCOVER_SVID_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			pdev->at_mode_established = false;
			pdev->alt_mode_req_rcv = false;
			pdev->alt_mode_req_send = false;
			pdev->alt_mode_state = PE_DFP_VDM_Init;
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			pr_info("NACK RECEIVED\n");
		}
		break;
	case DFP_SVIDs_Request:
		pr_info("SEND_COMMAND: DISCOVER_MODES\n");
		usbpd_send_vdm_cmd(pUsbpd_prtlyr, VDM, CMD_DISCOVER_MODES, pdev->svid_mode);
		pdev->alt_mode_state = DFP_SVIDs_Modes_Good_Crc_Rcvd;
		break;
	case DFP_SVIDs_Modes_Good_Crc_Rcvd:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			sii_timer_start(&(pdev->usbpd_vdm_resp_tmr));
			pdev->alt_mode_state = DFP_DISCOVER_SVID_MODES_ACK;

			if (pdev->svid_mode == 0x02) {
				pr_info("DP Acknowledged Successfully\n");
				pdev->at_mode_established = false;
				pdev->alt_mode_req_rcv = false;
				pdev->alt_mode_req_send = false;
				pdev->alt_mode_state = PE_DFP_VDM_Init;
				sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
				if (test_bit
				    (DISCOVER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
					work = 1;
				}
			}
		}
		break;
	case DFP_DISCOVER_SVID_MODES_ACK:
		if (test_bit(DISCOVER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			clear_bit(DISCOVER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			work = 1;
			pdev->alt_mode_state = drp_modes_Request;
		} else if (test_bit(DISCOVER_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			clear_bit(DISCOVER_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			sii_timer_stop(&(pdev->usbpd_vdm_resp_tmr));
			pr_info("NACK RECEIVED\n");
			pdev->at_mode_established = false;
			pdev->alt_mode_req_rcv = false;
			pdev->alt_mode_req_send = false;
		}
		break;
	case drp_modes_Request:
		pr_debug("drp_modes_Request\n");
		pr_info("SEND_COMMAND: DISCOVER_ENTER_MODE\n");
		usbpd_send_vdm_cmd(pUsbpd_prtlyr, VDM, CMD_ENTER_MODE, pdev->svid_mode);
		pdev->alt_mode_state = DFP_SVIDs_Enter_Modes_Good_Crc_Rcvd;
		break;
	case DFP_SVIDs_Enter_Modes_Good_Crc_Rcvd:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->alt_mode_state = DFP_SVIDs_ENTER_MODE_ACK_RCVD;
			sii_timer_start(&(pdev->usbpd_vdm_mode_entry_tmr));
			if (test_bit(ENTER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				pr_debug("ENTER_MODE_ACK_RCVD\n");
				work = 1;
			} else if (test_bit(ENTER_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				pr_debug("ENTER_MODE_NACK_RCVD\n");
				work = 1;
			}
		}
		break;
	case DFP_SVIDs_ENTER_MODE_ACK_RCVD:
		if (test_bit(ENTER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			sii_timer_stop(&(pdev->usbpd_vdm_mode_entry_tmr));
			pdev->alt_mode_state = drp_mode_Entry_ACKed;
			clear_bit(ENTER_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			work = 1;
		} else if (test_bit(ENTER_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			clear_bit(ENTER_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs);
			sii_timer_stop(&(pdev->usbpd_vdm_mode_entry_tmr));
			pr_info("NACK RECEIVED\n");
			pdev->at_mode_established = false;
			pdev->alt_mode_req_rcv = false;
			pdev->alt_mode_req_send = false;
		}
		break;
	case drp_mode_Entry_ACKed:
		pr_info("Enter mode done\n");
		pdev->at_mode_established = true;
		pdev->alt_mode_req_send = false;
		pdev->alt_mode_req_rcv = false;
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(drv_context, PD_DFP_ENTER_MODE_DONE, 0x00, NULL);
#endif
		break;

	case PE_DFP_VDM_Mode_Exit_Request:
		 /*DPM*/ pr_debug("PE_DFP_VDM_Mode_Exit_Request\n");
		usbpd_send_vdm_cmd(pUsbpd_prtlyr, VDM, CMD_EXIT_MODE, pdev->svid_mode);
		pdev->alt_mode_state = PE_Exit_Mode_Good_Crc_Received;
		break;

	case PE_Exit_Mode_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			sii_timer_start(&(pdev->usbpd_vdm_exit_tmr));
			pdev->alt_mode_state = PE_drp_mode_Wait_Exit_ACKed;
		}
		break;

	case PE_drp_mode_Wait_Exit_ACKed:
		if (test_bit(EXIT_MODE_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			sii_timer_stop(&(pdev->usbpd_vdm_exit_tmr));
			clear_bit(EXIT_MODE_ACK_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->alt_mode_state = PE_drp_mode_Exit_ACKed;
			work = 1;
		} else if (test_bit(EXIT_MODE_NACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
			clear_bit(EXIT_MODE_NACK_RCVD, &pdev->intf.param.sm_cmd_inputs);
			sii_timer_stop(&(pdev->usbpd_vdm_exit_tmr));
			pdev->exit_mode_req_send = false;
			pdev->at_mode_established = false;
			pdev->alt_mode_state = PE_DFP_VDM_Init;
		}
		break;
	case PE_drp_mode_Exit_ACKed:
		 /*DPM*/ pr_info("EXIT MODE DONE\n");
		sii_timer_stop(&(pdev->usbpd_vdm_exit_tmr));
		pdev->exit_mode_req_send = false;
		pdev->at_mode_established = false;
		pdev->alt_mode_state = PE_DFP_VDM_Init;
		/*si_enable_switch_control(pdev->drv_context, PD_TX, false); */
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(pdev->drv_context, PD_DFP_EXIT_MODE_DONE, 0x00, NULL);
#endif
		break;
	case DFP_SVIDs_MODES_NAK:
	case DFP_SVIDs_EXIT_NAK:
	case DFP_SVIDs_ENTERY_NAK:
	case DFP_SVIDs_SVID_NAK:
	case DFP_SVIDs_IDENTITY_NAK:
		pdev->at_mode_established = false;
		pdev->alt_mode_req_rcv = false;
		pdev->alt_mode_req_send = false;
		pdev->alt_mode_state = PE_DFP_VDM_Init;
		pr_info("ALT MODE EXIT\n");
		work = 1;
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(drv_context, PD_DFP_EXIT_MODE_DONE, 0x00, NULL);
#endif
		break;

	default:
		break;

	}
	return work;
}

bool sii_src_dr_swap_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("DR SWAP POLICY ENGINE -  \t");

	pr_info("Swap state:%d\n", pdev->dr_swap_state);
	pdev->pr_swap.done = false;

	switch (pdev->dr_swap_state) {
	case PE_SRC_DR_Swap_Init:
		if (pdev->drv_context->pUsbpd_dp_mngr->dr_swap_supp) {
			pdev->dr_swap.done = false;
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Send_DR_Swap;
		} else {
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Reject_DR_Swap;
		}
		work = 1;
		break;
	case PE_DRS_DFP_UFP_Send_DR_Swap:
		pr_info("SEND_COMMAND: DR_SWAP\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__DR_SWAP);
		pdev->dr_swap_state = PE_SWAP_Wait_Good_Crc_Received;
		break;
	case PE_SWAP_Wait_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->dr_swap_state = PE_PRS_Wait_Accept_DR_Swap;
			sii_timer_start(&(pdev->usbpd_inst_sendr_resp_tmr));
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("ACCEPT_RCVD\n");
				work = 1;
			} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("REJECT_RCVD\n");
				work = 1;
			} else if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("WAIT_RCVD\n");
				work = 1;
			}
		}
		break;
	case PE_PRS_Wait_Accept_DR_Swap:
		if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Change_to_UFP;
			work = 1;
		} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
			pr_info("DR SWAP REJECTED\n");
		} else if (test_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(WAIT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
			pr_info("PEER IS NOT READY FOR DR_SWAP\n");
		}
		break;

	case PE_DRS_DFP_UFP_Change_to_UFP:
		change_drp_data_role(pdev);
		pr_info("DR SWAP COMPLETED\n");
		pdev->dr_swap.req_send = false;
		/*pdev->dr_swap_state = PE_SRC_DR_Swap_Init; */
		pdev->dr_swap.done = true;
		pdev->dr_swap.req_rcv = false;
#if defined(I2C_DBG_SYSFS)
		usbpd_event_notify(drv_context, PD_DR_SWAP_DONE, 0x00, NULL);
#endif
		si_update_pd_status(pdev);
		/*work = 1; */
		break;

	case PE_DRS_DFP_UFP_Evaluate_DR_Swap:
		if (pdev->drv_context->pUsbpd_dp_mngr->dr_swap_supp) {
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Accept_DR_Swap;
			work = 1;
		} else {
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Reject_DR_Swap;
			work = 1;
		}
		break;

	case PE_DRS_DFP_UFP_Accept_DR_Swap:
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__ACCEPT);
		pdev->dr_swap_state = PE_Wait_Accept_Good_Crc_Received;
		break;

	case PE_Wait_Accept_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->dr_swap_state = PE_DRS_DFP_UFP_Change_to_UFP;
			work = 1;
		} else {
		}
		break;
	case PE_DRS_Swap_exit:
		pdev->dr_swap.req_send = false;
		pdev->dr_swap.req_rcv = false;
		pdev->dr_swap.done = false;
		pr_info("DR SWAP EXITED\n");
		break;
	case PE_DRS_DFP_UFP_Reject_DR_Swap:
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__REJECT);
		pdev->dr_swap_state = PE_SRC_Wait_Reject_Good_Crc_Received;
		break;
	case PE_SRC_Wait_Reject_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pr_info("DR SWAP NOT SUPPORTED\n");
			pdev->dr_swap.req_send = false;
			pdev->dr_swap.in_progress = false;
			pdev->dr_swap.req_rcv = false;
			pdev->dr_swap.done = false;
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_DR_SWAP_EXIT, 0x00, NULL);
#endif
		}
		break;
	default:
		break;
	}
	return work;
}

bool sii_src_vconn_swap_mode_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_info("VCONN POLICY ENGINE -  \t");

	pr_info("vconn state:%d\n", pdev->vconn_swap_state);
	pdev->dr_swap.done = false;

	switch (pdev->vconn_swap_state) {
	case PE_VCS_DFP_Send_Swap:
		pr_info("SEND_COMMAND: VCONN_SWAP\n");
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__VCONN_SWAP);
		pdev->vconn_swap_state = PE_SWAP_Wait_Good_Crc_Received;
		break;

	case PE_SWAP_Wait_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			sii_timer_start(&(pdev->usbpd_inst_sendr_resp_tmr));
			pdev->vconn_swap_state = PE_PRS_Wait_Accept_VCONN_Swap;
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("ACCEPT_RCVD\n");
				work = 1;
			}
		}
		break;
	case PE_PRS_Wait_Accept_VCONN_Swap:
		if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {

			clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));

			if (pdev->vconn_is_on) {
				pdev->vconn_swap_state = PE_VCS_DFP_Wait_for_UFP_VCONN;
				work = 1;
			} else {
				pdev->vconn_swap_state = PE_VCS_DFP_Turn_ON_VCONN;
				work = 1;
			}
		}
		break;

	case PE_VCS_DFP_Wait_for_UFP_VCONN:
		sii_timer_start(&(pdev->usbpd_inst_vconn_on_tmr));
		pdev->vconn_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
		break;

	case PE_SWAP_Wait_Ps_Rdy_Received:
		pr_debug("PE_VCONN_Wait_Ps_Rdy_Received\n");
		if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			clear_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs);
			sii_timer_stop(&(pdev->usbpd_inst_vconn_on_tmr));
			pdev->vconn_swap_state = PE_VCS_DFP_Turn_Off_VCONN;
			work = 1;
		}
		break;
	case PE_VCS_DFP_Turn_Off_VCONN:
		pdev->vconn_swap.done = true;
		pdev->vconn_swap.req_send = false;
		pdev->vconn_swap.enable = false;
		/*si_update_pd_status(pdev); */
		break;
	case PE_VCS_DFP_Turn_ON_VCONN:
		pdev->vconn_swap.enable = true;
		/*si_update_pd_status(pdev); */
		pdev->vconn_swap_state = PE_VCS_DFP_Send_PS_Rdy;
		work = 1;
		break;
	case PE_VCS_DFP_Send_PS_Rdy:
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PS_RDY);
		pdev->vconn_swap_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
		break;
	case PE_SRC_Wait_Src_rdy_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
			pdev->vconn_swap.done = true;
			pdev->vconn_swap.req_send = false;
			pr_info("VCONN SWAP COMPLETED\n");
		}
		break;
	default:
		break;
	}
	return work;
}

bool sii_src_pr_swap_engine(struct sii_usbp_policy_engine *pdev)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_typec *ptypec_dev = (struct sii_typec *)drv_context->ptypec;
	struct sii_usbpd_protocol *pUsbpd_prtlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	bool work = 0;

	pr_debug("SWAP POLICY ENGINE -  \t");

	pr_debug("SWap state:%d\n", pdev->pr_swap_state);
	pdev->dr_swap.done = false;

	switch (pdev->pr_swap_state) {
	case PE_SRC_Swap_Init:
		if (pdev->drv_context->pUsbpd_dp_mngr->pr_swap_supp) {
			usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PR_SWAP);
			pdev->pr_swap.in_progress = true;
			pdev->pr_swap_state = PE_SWAP_Wait_Good_Crc_Received;
		} else {
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Reject_PR_Swap;
			work = 1;
		}
		break;

	case PE_SWAP_Wait_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap_state = PE_PRS_Wait_Accept_PR_Swap;
			sii_timer_start(&(pdev->usbpd_inst_sendr_resp_tmr));
			if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("ACCEPT_RCVD\n");
				work = 1;
			} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("REJECT_RCVD\n");
				work = 1;
			}
			pdev->pr_swap.in_progress = true;
		} else {
			pdev->pr_swap.in_progress = false;
		}
		break;

	case PE_PRS_Wait_Accept_PR_Swap:
		if (test_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(ACCEPT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Transition_to_off;
			work = 1;
			pdev->pr_swap.in_progress = true;
		} else if (test_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
			clear_bit(REJECT_RCVD, &pdev->intf.param.sm_cmd_inputs);
			pr_info("PR SWAP REJECTED FROM PEER\n");
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = false;
			pdev->pr_swap_state = PE_SRC_Swap_Init;
		}
		break;
	case PE_PRS_SRC_SNK_Evaluate_Swap:
		pr_debug("PE_PRS_SRC_SNK_Evaluate_Swap\n");
		if (pdev->drv_context->pUsbpd_dp_mngr->pr_swap_supp)
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Accept_Swap;
		else
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Reject_PR_Swap;
		work = 1;
		break;
	case PE_PRS_SRC_SNK_Accept_Swap:
		pr_debug("PE_PRS_SRC_SNK_Accept_Swap\n");
		pdev->pr_swap.in_progress = true;
		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__ACCEPT);
		pdev->pr_swap_state = PE_Wait_Accept_Good_Crc_Received;
		break;

	case PE_Wait_Accept_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Transition_to_off_wait;
			work = 1;
		} else {
		}
		break;
	case PE_PRS_SRC_SNK_Transition_to_off_wait:
		pr_debug("PE_PRS_SRC_SNK_Transition_to_off\n");

		/*msleep(tSnkTransition); */
		/*scheduling issues are observed with msleep */
		sii_timer_start(&(pdev->usbpd_tSnkTransition_tmr));
		pdev->pr_swap_state = PE_PRS_SRC_SNK_Transition_to_off;
		break;
	case PE_PRS_SRC_SNK_Transition_to_off:
		sii_timer_start(&(pdev->usbpd_inst_source_act_tmr));

		if (!ptypec_dev->typecDrp)
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Source_off;
		else
			pdev->pr_swap_state = PE_PRS_SRC_SNK_Assert_Rd;

		work = 1;
		break;
	case PE_PRS_SRC_SNK_Source_off:
		pr_debug("PE_PRS_SRC_SNK_Source_off\n");
		/*      sii_timer_stop(&
		   (pdev->usbpd_inst_source_act_tmr)); */
		change_drp_pwr_role(pdev);
		pr_info("SEND_COMMAND: PS_RDY\n");

		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__PS_RDY);
		pdev->pr_swap_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
		break;

	case PE_SRC_Wait_Src_rdy_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			sii_timer_start(&(pdev->usbpd_inst_ps_source_on_tmr));
			pdev->pr_swap_state = PE_SWAP_Wait_Ps_Rdy_Received;
			if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("PS_RDY_RCVD\n");
				work = 1;
			}
		}
		break;
	case PE_SWAP_Wait_Ps_Rdy_Received:
		pr_debug("PE_SWAP_Wait_Ps_Rdy_Received\n");
		if (test_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
			clear_bit(PS_RDY_RCVD, &pdev->intf.param.sm_cmd_inputs);
			sii_timer_stop(&(pdev->usbpd_inst_ps_source_on_tmr));
			pr_info("PR SWAP COMPLETED\n");
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_PR_SWAP_DONE, 0x00, NULL);
#endif
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = true;
			si_update_pd_status(pdev);
			work = 0;
		}
		break;
	case PE_PRS_SRC_SNK_Assert_Rd:
		pr_debug("PE_PRS_SRC_SNK_Assert_Rd\n");
		pdev->pr_swap_state = PE_PRS_SRC_SNK_Source_off;
		work = 1;

		break;
	case PE_PRS_Swap_exit:
		pdev->pr_swap.req_send = false;
		pdev->pr_swap.in_progress = false;
		pdev->pr_swap.req_rcv = false;
		pdev->pr_swap.done = false;
		pr_info("PR SWAP EXIT\n");
		break;
	case PE_PRS_SRC_SNK_Reject_PR_Swap:
		pr_info("SEND_COMMAND: REJECT\n");

		usbpd_xmit_ctrl_msg(pUsbpd_prtlyr, CTRL_MSG__REJECT);
		pdev->pr_swap_state = PE_SRC_Wait_Reject_Good_Crc_Received;
		break;
	case PE_SRC_Wait_Reject_Good_Crc_Received:
		if (pdev->tx_good_crc_received) {
			pdev->tx_good_crc_received = 0;
			pr_info("PR SWAP NOT SUPPORTED\n");
			pdev->pr_swap.req_send = false;
			pdev->pr_swap.in_progress = false;
			pdev->pr_swap.req_rcv = false;
			pdev->pr_swap.done = false;
			pdev->pr_swap_state = PE_SRC_Swap_Init;
			work = 1;
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_PR_SWAP_EXIT, 0x00, NULL);
#endif
		}
		break;
	default:
		break;
	}
	return work;
}

void si_custom_msg_xmit(struct sii_usbp_policy_engine *pUsbpd)
{
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pUsbpd->drv_context;
	struct sii_usbpd_protocol *pUsbpd_protlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;

	send_custom_vdm_message(pUsbpd_protlyr, 0xF, pUsbpd->vdm_cnt);

	pUsbpd->vdm_cnt++;
	if (pUsbpd->vdm_cnt == 30)
		pUsbpd->vdm_cnt = 0;

	msleep(3000);

}

void source_policy_engine(WORK_STRUCT *w)
{
	struct sii_usbp_policy_engine *pdev = container_of(w, struct sii_usbp_policy_engine,
							   pd_dfp_work);
	struct sii70xx_drv_context *drv_context = (struct sii70xx_drv_context *)pdev->drv_context;
	struct sii_usbpd_protocol *pUsbpd_protlyr =
	    (struct sii_usbpd_protocol *)drv_context->pUsbpd_prot;
	uint8_t pdo_value, work = 0;
	bool result;

	if (!pdev) {
		pr_debug("Not able to find address\n");
		return;
	}
	if (!pdev->pd_connected) {
		pr_info("DFP not Connected\n");
		return;
	}
	pr_info("SOURCE -> UNLOCK\n");
	if (!down_interruptible(&drv_context->isr_lock)) {
		pr_info("SOURCE LOCK - %x\n", pdev->state);
		switch (pdev->state) {
		case PE_SRC_Disabled:
			pr_debug("PE_SRC_Disabled\n");
			/*Set to default 5v as per spec */
			result = set_pwr_params_default(pdev);
			break;

		case PE_SRC_Discovery:
			pr_debug("PE_SRC_Discovery\n");
			sii_timer_start(&(pdev->usbpd_source_cap_tmr));
			break;

		case PE_SRC_Transition_to_default:
			pr_debug("PE_SRC_Transition_to_default\n");
			result = set_pwr_params_default(pdev);

			work = 1;
			pdev->next_state = PE_SRC_Startup;

			sii_timer_start(&(pdev->usbpd_inst_no_resp_tmr));
			break;

		case PE_SRC_Startup:
			pr_debug("PE_SRC_Startup\n");

			/*if(test_bit(PR_SWAP_DONE, &pdev->drv_context->
			   ptypec->inputs)) {
			   pr_info("PR_SWAP_COMPLETED\n");
			   clear_bit(PR_SWAP_DONE, &pdev->drv_context->
			   ptypec->inputs);
			   sii_timer_start(&pdev->usbpd_source_swap_tmr);
			   } */
			pdev->caps_counter = 0;
			pdev->custom_msg = 0;
			/*usbpd_core_reset(pUsbpd_prot); */
			work = 1;
			pdev->next_state = PE_SRC_Send_Capabilities;
			break;

		case PE_SRC_Send_Capabilities:
			pr_debug("PE_SRC_Send_Capabilities\n");
			msleep(100);	/*kept for nuvuton synchronization */
			pdo_value = sii_usbpd_get_src_cap(pdev,
							  pUsbpd_protlyr->send_msg, FIXED_SUPPLY);
			sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
					      BIT_MSK__PDCC24INT3__REG_PDCC24_INTR28);
			/*due to bugi n hardware */
			pr_info("SEND_COMMAND: SRC_CAP\n");

			result = sii_usbpd_xmit_data_msg(pUsbpd_protlyr, SRCCAP,
							 pUsbpd_protlyr->send_msg, pdo_value);

			pdev->caps_counter++;
			pdev->next_state = PE_SWAP_Wait_Good_Crc_Received;
			work = 0;
			break;

		case PE_SWAP_Wait_Good_Crc_Received:
			if (pdev->tx_good_crc_received) {
				pr_debug("GOOD CRC received\n");
				pdev->tx_good_crc_received = 0;
				sii_timer_stop(&pdev->usbpd_inst_no_resp_tmr);
				sii_timer_stop(&(pdev->usbpd_source_cap_tmr));
				pdev->hard_reset_counter = 0;
				pdev->caps_counter = 0;
				sii_timer_start(&(pdev->usbpd_inst_sendr_resp_tmr));
				pdev->next_state = PE_SRC_Wait_Req_Msg_Received;
				/*sometime both interupts are coming
				   and both queue
				   works are executing at a same time */
				if (test_bit(REQ_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
					pr_debug("REQ_RCVD\n");
					work = 1;
				}
			} else {
				pr_debug("GOOD CRC not received\n");
				pdev->next_state = PE_SRC_Discovery;
				work = 1;
			}
			break;

		case PE_SRC_Wait_Req_Msg_Received:
			if (test_bit(REQ_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				sii_timer_stop(&(pdev->usbpd_inst_sendr_resp_tmr));
				clear_bit(REQ_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->next_state = PE_SRC_Negotiate_Capability;
				work = 1;
			}
			break;

		case PE_SRC_Negotiate_Capability:
			pr_debug("PE_SRC_Negotiate_Capability\n");

			if (test_bit(CAN_NOT_BE_MET, &pdev->intf.param.sm_cmd_inputs)) {
				pdev->next_state = PE_SRC_Capability_Response;
				work = 1;
			} else {
				pdev->next_state = PE_SRC_Transition_Supply;
				work = 1;
			}
#if defined(I2C_DBG_SYSFS)
			usbpd_event_notify(drv_context, PD_POWER_LEVELS, 0x00, NULL);
#endif
			break;

		case PE_SRC_Transition_Supply:
			pr_debug("sm_work: PE_SRC_Transition_Supply\n");
			sii_timer_start(&(pdev->usbpd_inst_source_act_tmr));
			if (pdev->drv_context->pUsbpd_dp_mngr->go_to_min) {
				/*rewuested by device policy manager */
				pr_info("SEND_COMMAND: GOTO_MIN\n");
				result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__GO_TO_MIN);
			} else {
				pr_info("SEND_COMMAND: ACCEPT\n");
				result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__ACCEPT);
			}
			pdev->next_state = PE_SRC_Wait_Accept_Good_Crc_Received;
			break;

		case PE_SRC_Wait_Accept_Good_Crc_Received:
			pr_debug("PE_SRC_Wait_Accept_Good_Crc_Received\n");
			if (pdev->tx_good_crc_received) {
				pdev->tx_good_crc_received = 0;
				/*msleep(tSnkTransition); */
				sii_timer_start(&(pdev->usbpd_tSnkTransition_tmr));
				pdev->next_state = PE_SRC_Wait_Ps_Rdy_sent;
			} else {
				pr_err("GOOD CRC not received -2\n");
				usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__SOFT_RESET);
				pdev->next_state = PE_SRC_Wait_sft_rst_Good_Crc_Received;
			}
			break;
		case PE_SRC_Wait_Ps_Rdy_sent:
			pr_info("SEND_COMMAND: PS_RDY\n");
			result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__PS_RDY);
			pdev->next_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
			break;

		case PE_SRC_Capability_Response:
			pr_debug("PE_SRC_Capability_Response\n");
			if (test_bit(CAN_NOT_BE_MET, &pdev->intf.param.sm_cmd_inputs)) {
				pr_info("SEND_COMMAND: REJECT\n");
				result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__REJECT);
			} else if (test_bit(LATER_GB, &pdev->intf.param.sm_cmd_inputs)) {
				pr_debug("set the bit later\n");
				clear_bit(LATER_GB, &pdev->intf.param.sm_cmd_inputs);
				pr_info("SEND_COMMAND: WAIT\n");
				result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__WAIT);
			}
			pdev->next_state = PE_SRC_Wait_cap_met_Good_Crc_Received;
			break;
		case PE_SRC_Wait_Src_rdy_Good_Crc_Received:
			pr_debug("PE_SRC_WaitSrc_rdy_good_Crc_Received\n");
			if (pdev->tx_good_crc_received) {
				pdev->next_state = PE_SRC_Ready;
				work = 1;
				pdev->tx_good_crc_received = 0;
				pr_info("POWER CONTRACT ESTABLISHED\n");
			} else {
				pr_err("GOOD CRC not received\n");
			}
			break;

		case PE_SRC_Wait_sft_rst_Good_Crc_Received:
			pr_debug("PE_SFTWaitSrc_rdy_good_Crc_Received\n");
			if (pdev->tx_good_crc_received) {
				pdev->next_state = PE_SRC_Discovery;
				work = 1;
				pr_info("soft reset done\n");
				pdev->tx_good_crc_received = 0;
			} else {
				pr_err("GOOD CRC not received - 3\n");
				pdev->next_state = PE_SRC_Hard_Reset;
				work = 1;
			}
			break;

		case PE_SRC_Ready:
			pr_info("SRC READY\n");
			if (test_bit(GOTOMIN_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(GOTOMIN_RCVD, &pdev->intf.param.sm_cmd_inputs);
				msleep(100);
				pdev->alt_mode_req_send = true;
			}

			/************************VCONN SWAP*******************/
			if (pdev->vconn_swap.req_send) {
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_vconn_swap_mode_engine(pdev))
					work = 1;
				break;
			}

			/******************************************************/
			/*optional to start activity timer here */
			/*sii_timer_start(&(pdev->usbpd_inst_source_act_tmr)); */
			/* this should be from device policy manager */
			if (pdev->dr_swap.done) {
				if (pdev->custom_msg) {
					si_custom_msg_xmit(pdev);
					pdev->next_state = PE_SRC_Ready;
					work = 1;
					break;
				}
				pdev->dr_swap.done = false;
			}
			if (test_bit(BIST_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(BIST_RCVD, &pdev->intf.param.sm_cmd_inputs);
				drv_context->irq_disable = true;
			}

			if (test_bit(SOFT_RESET_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(SOFT_RESET_RCVD, &pdev->intf.param.sm_cmd_inputs);
				usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__ACCEPT);
			}
			/*****************************PR_SWAP******************/
			if (pdev->pr_swap.req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_pr_swap_engine(pdev))
					work = 1;
				break;
			}
			if (test_bit(PR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(PR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->pr_swap.req_rcv = 1;
				pdev->pr_swap_state = PE_PRS_SRC_SNK_Evaluate_Swap;
			}
			if (pdev->pr_swap.req_rcv == 1) {
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_pr_swap_engine(pdev))
					work = 1;
				break;
			}
			/**************************ALT MODE*******************/
			if (pdev->alt_mode_req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SRC_Ready;
				pr_info("Alt mode state PP:%d\n", pdev->alt_mode_state);
				if (sii_src_alt_mode_engine(pdev))
					work = 1;
				break;
			}
			if (test_bit(SVDM_DISC_IDEN_ACK_RCVD, &pdev->intf.param.svdm_sm_inputs)) {
				clear_bit(SVDM_DISC_IDEN_ACK_RCVD,
					  &pdev->intf.param.svdm_sm_inputs);
				pdev->alt_mode_state = DFP_Identity_Request;
				work = 1;
				pdev->alt_mode_req_rcv = 1;
			}
			if (pdev->alt_mode_req_rcv == 1) {
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_alt_mode_engine(pdev))
					work = 1;
				break;
			}
			/***************************EXIT MODE*****************/
			if (pdev->exit_mode_req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_alt_mode_engine(pdev))
					work = 1;
				break;
			}
			/*****************************DR SWAP*****************/
			if (pdev->dr_swap.req_send) {
				/* requested from sysfs */
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_dr_swap_engine(pdev))
					work = 1;
				break;
			}
			if (test_bit(DR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(DR_SWAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdev->dr_swap_state = PE_DRS_DFP_UFP_Evaluate_DR_Swap;
				work = 1;
				pdev->dr_swap.req_rcv = 1;
			}
			if (pdev->dr_swap.req_rcv == 1) {
				pdev->next_state = PE_SRC_Ready;
				if (sii_src_dr_swap_engine(pdev))
					work = 1;
				break;
			}
			/******************************************************/
			/* this should come from device policy manager */
			if (test_bit(GET_SINK_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(GET_SINK_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				if (pdev->drv_context->pUsbpd_dp_mngr->get_sink_cap) {
					work = 1;
					pdev->next_state = PE_SRC_Get_Sink_Cap;
				}
				break;
			}
			/* this should come from device policy manager */
			if (test_bit(REQ_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				clear_bit(REQ_RCVD, &pdev->intf.param.sm_cmd_inputs);
				work = 1;
				pdev->next_state = PE_SRC_Negotiate_Capability;
			}
			if (test_bit(GET_SOURCE_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs)) {
				/*pr_debug("get source cap received\n"); */
				clear_bit(GET_SOURCE_CAP_RCVD, &pdev->intf.param.sm_cmd_inputs);
				pdo_value = sii_usbpd_get_src_cap(pdev,
								  pUsbpd_protlyr->send_msg,
								  FIXED_SUPPLY);
				pr_info("SEND_COMMAND: SRC_CAP\n");

				result = sii_usbpd_xmit_data_msg(pUsbpd_protlyr,
								 SRCCAP,
								 pUsbpd_protlyr->send_msg,
								 pdo_value);
				work = 1;
				pdev->next_state = PE_SNK_Get_Source_Cap;
				break;
			}
			break;
		case PE_SRC_Src_rdy_Good_Crc_Received:
			pr_debug("PE_SRC_WaitSrc_rdy_good_Crc_Received\n");
			if (pdev->tx_good_crc_received) {
				pdev->tx_good_crc_received = 0;
				pdev->next_state = PE_SRC_Ready;
				work = 1;
			}
			break;

		case PE_SRC_Give_Source_Cap:
			pr_debug("PE_SRC_Give_Source_Cap\n");
			pdo_value = sii_usbpd_get_src_cap(pdev,
							  pUsbpd_protlyr->send_msg, FIXED_SUPPLY);
			pr_info("SEND_COMMAND: SRC_CAP\n");
			result = sii_usbpd_xmit_data_msg(pUsbpd_protlyr,
							 SRCCAP, pUsbpd_protlyr->send_msg,
							 pdo_value);
			pdev->next_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
			break;

		case PE_SRC_Get_Sink_Cap:
			pr_info("SEND_COMMAND: GET_SNK_CAP\n");
			result = usbpd_xmit_ctrl_msg(pUsbpd_protlyr, CTRL_MSG__GET_SINK_CAP);
			sii_timer_start(&pdev->usbpd_inst_sendr_resp_tmr);
			pdev->next_state = PE_SRC_Wait_Src_rdy_Good_Crc_Received;
			break;

		case PE_SRC_Hard_Reset:
			pr_debug("PE_SRC_Hard_Reset\n");
			result = send_hardreset(pdev);
			if (result) {
				pdev->hard_reset_counter++;
				pdev->next_state = PE_SRC_Wait_HR_Good_Crc_Received;
			} else {
				pdev->next_state = PE_SRC_Disabled;
				work = 1;
			}
			break;
		case PE_SRC_Wait_HR_Good_Crc_Received:
			sii_platform_set_bit8(REG_ADDR__PDCTR0,
					      BIT_MSK__PDCTR0__RI_PRL_HCRESET_DONE_BY_PE_WP);
			pdev->next_state = PE_SRC_Transition_to_default;
			work = 1;
			break;
		default:
			break;
		}
		pdev->state = pdev->next_state;
		up(&drv_context->isr_lock);
	}
	if (work) {
		work = 0;
		wakeup_dfp_queue(drv_context);
	}
	pr_debug("SRC QUEUE COMPLETED\n");
}

void wakeup_dfp_queue(struct sii70xx_drv_context *drv_context)
{
	struct sii_usbp_policy_engine *pUsbpd =
	    (struct sii_usbp_policy_engine *)drv_context->pusbpd_policy;

	if (pUsbpd->pd_connected)
		sii_wakeup_queues(pUsbpd->dfp_work_queue,
				  &pUsbpd->pd_dfp_work, &pUsbpd->is_event, true);
	else
		pr_info("cable is disconnected\n");
}


void sii_reset_dfp(struct sii_usbp_policy_engine *pUsbpd)
{
	pr_info("RESET DFP\n");

	if (pUsbpd->usbpd_inst_sendr_resp_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_sendr_resp_tmr);
	if (pUsbpd->usbpd_inst_source_act_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_source_act_tmr);
	if (pUsbpd->usbpd_inst_ps_source_on_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_ps_source_on_tmr);
	if (pUsbpd->usbpd_source_cap_tmr)
		sii_timer_stop(&pUsbpd->usbpd_source_cap_tmr);
	if (pUsbpd->usbpd_inst_tbist_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_tbist_tmr);
	if (pUsbpd->usbpd_vdm_mode_entry_tmr)
		sii_timer_stop(&pUsbpd->usbpd_vdm_mode_entry_tmr);
	if (pUsbpd->usbpd_vdm_resp_tmr)
		sii_timer_stop(&pUsbpd->usbpd_vdm_resp_tmr);
	if (pUsbpd->usbpd_vdm_exit_tmr)
		sii_timer_stop(&pUsbpd->usbpd_vdm_exit_tmr);
	if (pUsbpd->usbpd_inst_vconn_on_tmr)
		sii_timer_stop(&pUsbpd->usbpd_inst_vconn_on_tmr);
	if (pUsbpd->usbpd_tSnkTransition_tmr)
		sii_timer_stop(&pUsbpd->usbpd_tSnkTransition_tmr);
	pUsbpd->state = PE_SRC_Startup;
	pUsbpd->pr_swap_state = PE_SRC_Swap_Init;
	pUsbpd->alt_mode_state = PE_DFP_VDM_Init;
	pUsbpd->src_vconn_swap_req = false;

	pUsbpd->exit_mode_req_send = false;
	pUsbpd->alt_mode_req_send = false;
	pUsbpd->alt_mode_req_rcv = false;
	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));
	pUsbpd->custom_msg = false;
	pUsbpd->vdm_cnt = 0;
	if (pUsbpd->at_mode_established == true) {
		pUsbpd->at_mode_established = false;
		/*si_enable_switch_control(pUsbpd->drv_context, PD_TX, false); */
	}
	pr_info("RESET DFP FINISHED\n");
}

bool usbpd_set_dfp_swap_init(struct sii_usbp_policy_engine *pUsbpd)
{
	bool ret = true;

	if (!pUsbpd) {
		pr_warn("%s: Error initialisation.\n", __func__);
		return false;
	}
	pr_debug("usbpd_set_dfp_swap_init\n");

	sii_platform_set_bit8(REG_ADDR__PDCC24INTM3, BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK29);

	sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK25);

	pUsbpd->pd_connected = true;

	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->vconn_swap, 0, sizeof(struct swap_config));

	pUsbpd->intf.param.sm_cmd_inputs = 0;
	pUsbpd->intf.param.svdm_sm_inputs = 0;
	pUsbpd->intf.param.uvdm_sm_inputs = 0;
	pUsbpd->intf.param.count = 0;

	set_pd_reset(pUsbpd->drv_context, false);

	pUsbpd->state = PE_SRC_Startup;
	pUsbpd->next_state = PE_SRC_Startup;

	pUsbpd->pr_swap_state = PE_SRC_Swap_Init;
	pUsbpd->alt_mode_state = PE_DFP_VDM_Init;
	pUsbpd->dr_swap_state = PE_SRC_DR_Swap_Init;
	pUsbpd->at_mode_established = false;
	pUsbpd->custom_msg = false;
	pUsbpd->vdm_cnt = 0;
	pUsbpd->vconn_swap_state = PE_VCS_DFP_Send_Swap;
	if (!usbpd_source_timer_create(pUsbpd)) {
		pr_debug("%s: Failed in timer create.\n", __func__);
		ret = false;
		goto exit;
	}
	pUsbpd->drv_context->drp_config = USBPD_DFP;
	wakeup_dfp_queue(pUsbpd->drv_context);
exit:	return ret;
}

bool usbpd_set_dfp_init(struct sii_usbp_policy_engine *pUsbpd)
{
	bool ret = true;

	if (!pUsbpd) {
		pr_warn("%s: Error initialisation.\n", __func__);
		return false;
	}
	pr_debug("usbpd_set_dfp_init\n");
	pUsbpd->drv_context->drp_config = USBPD_DFP;
	pUsbpd->drv_context->old_drp_config = USBPD_DFP;

	update_data_role(pUsbpd->drv_context, USBPD_DFP);
	update_pwr_role(pUsbpd->drv_context, USBPD_DFP);

	sii_platform_put_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_SPEC_REV, 0x01);

	sii_platform_set_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_DATA_ROLE);
	sii_platform_set_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_POWER_ROLE);

	sii_platform_set_bit8(REG_ADDR__PDCTR11,
			      BIT_MSK__PDCTR11__RI_PRL_RX_MSG_READ_HANDSHAKE |
			      BIT_MSK__PDCTR11__RI_PRL_RX_SKIP_GOODCRC_RDBUF);

	sii_platform_clr_bit8(REG_ADDR__PDCTR11,
			      BIT_MSK__PDCTR11__RI_PRL_RX_DISABLE_GOODCRC_DISCARD);

	sii_platform_set_bit8(REG_ADDR__PDCC24INTM3, BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK29);
	sii_platform_clr_bit8(REG_ADDR__PDCC24INTM3,
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24 |
			      BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK25);

	pUsbpd->pd_connected = true;

	memset(&pUsbpd->pr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->dr_swap, 0, sizeof(struct swap_config));
	memset(&pUsbpd->vconn_swap, 0, sizeof(struct swap_config));
	/*not doing anything in present firmware */
	/*sii_platform_wr_reg8(REG_ADDR__PDCC24INT2, 0xFF); */
	set_pd_reset(pUsbpd->drv_context, false);

	pUsbpd->state = PE_SRC_Startup;
	pUsbpd->pr_swap_state = PE_SRC_Swap_Init;
	pUsbpd->alt_mode_state = PE_DFP_VDM_Init;
	pUsbpd->dr_swap_state = PE_SRC_DR_Swap_Init;
	pUsbpd->at_mode_established = false;
	pUsbpd->vconn_swap_state = PE_VCS_DFP_Send_Swap;
	pUsbpd->custom_msg = false;
	if (!usbpd_source_timer_create(pUsbpd)) {
		pr_debug("%s: Failed in timer create.\n", __func__);
		ret = false;
		goto exit;
	}

	si_enable_switch_control(pUsbpd->drv_context, PD_TX, true);

	sii_platform_clr_bit8(REG_ADDR__PDCTR11, BIT_MSK__PDCTR11__RI_PRL_RX_SKIP_GOODCRC_RDBUF);
	wakeup_dfp_queue(pUsbpd->drv_context);
exit:	return ret;
}

void usbpd_dfp_exit(struct sii_usbp_policy_engine *pUsbpd)
{
	pUsbpd->custom_msg = 0;
	usbpd_source_timer_delete(pUsbpd);
/*destroy_workqueue(&pUsbpd->pd_dfp_work);*/
}
