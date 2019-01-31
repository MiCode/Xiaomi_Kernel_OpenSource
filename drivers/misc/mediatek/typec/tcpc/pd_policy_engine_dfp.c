/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Power Delivery Policy Engine for DFP
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

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0] Figure 8-64 DFP to UFP VDM Discover Identity State Diagram
 */

void pe_dfp_ufp_vdm_identity_request_entry(struct pd_port *pd_port)
{
	pd_send_vdm_discover_id(pd_port, TCPC_TX_SOP);
}

void pe_dfp_ufp_vdm_identity_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_id(pd_port, true);
}

void pe_dfp_ufp_vdm_identity_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_id(pd_port, false);
}

/*
 * [PD2.0] Figure 8-65 DFP VDM Discover Identity State Diagram
 */

void pe_dfp_cbl_vdm_identity_request_entry(struct pd_port *pd_port)
{
	pd_port->pe_data.discover_id_counter++;
	pd_send_vdm_discover_id(pd_port, TCPC_TX_SOP_PRIME);
}

void pe_dfp_cbl_vdm_identity_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_inform_cable_id(pd_port, false);
}

void pe_dfp_cbl_vdm_identity_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_inform_cable_id(pd_port, false);
}

/*
 * [PD2.0] Figure 8-66 DFP VDM Discover SVIDs State Diagram
 */

void pe_dfp_vdm_svids_request_entry(struct pd_port *pd_port)
{
	pd_send_vdm_discover_svids(pd_port, TCPC_TX_SOP);
}

void pe_dfp_vdm_svids_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_svids(pd_port, true);
}

void pe_dfp_vdm_svids_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_svids(pd_port, false);
}

/*
 * [PD2.0] Figure 8-67 DFP VDM Discover Modes State Diagram
 */

void pe_dfp_vdm_modes_request_entry(struct pd_port *pd_port)
{
	pd_send_vdm_discover_modes(pd_port, TCPC_TX_SOP, pd_port->mode_svid);
}

void pe_dfp_vdm_modes_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_modes(pd_port, true);
}

void pe_dfp_vdm_modes_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_modes(pd_port, false);
}

/*
 * [PD2.0] Figure 8-68 DFP VDM Mode Entry State Diagram
 */

void pe_dfp_vdm_mode_entry_request_entry(struct pd_port *pd_port)
{
	pd_send_vdm_enter_mode(pd_port, TCPC_TX_SOP,
		pd_port->mode_svid, pd_port->mode_obj_pos);
}

void pe_dfp_vdm_mode_entry_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_enter_mode(pd_port, true);
}

void pe_dfp_vdm_mode_entry_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_enter_mode(pd_port, false);
}

/*
 * [PD2.0] Figure 8-69 DFP VDM Mode Exit State Diagram
 */

void pe_dfp_vdm_mode_exit_request_entry(struct pd_port *pd_port)
{
	pd_send_vdm_exit_mode(pd_port, TCPC_TX_SOP,
		pd_port->mode_svid, pd_port->mode_obj_pos);
}

void pe_dfp_vdm_mode_exit_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_exit_mode(pd_port);
}

/*
 * [PD2.0] Figure 8-70 DFP VDM Attention State Diagram
 */

void pe_dfp_vdm_attention_request_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_attention(pd_port);
}

/*
 * [PD2.0] Figure 8-83 DFP Cable Soft Reset or Cable Reset State Diagram
 */

#ifdef CONFIG_PD_DFP_RESET_CABLE

void pe_dfp_cbl_send_soft_reset_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG_OR_TX_FAILED(pd_port);

	pd_send_cable_soft_reset(pd_port);
}

void pe_dfp_cbl_send_cable_reset_entry(struct pd_port *pd_port)
{
	/* TODO : we don't do cable reset now */
}

#endif	/* CONFIG_PD_DFP_RESET_CABLE */

/*
 * [PD2.0] Display Port
 */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

void pe_dfp_vdm_dp_status_update_request_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_send_dp_status_update(pd_port);
}

void pe_dfp_vdm_dp_status_update_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_dp_status_update(pd_port, true);
}

void pe_dfp_vdm_dp_status_update_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_dp_status_update(pd_port, false);
}

void pe_dfp_vdm_dp_configuration_request_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_send_dp_configuration(pd_port);
}

void pe_dfp_vdm_dp_configuration_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_dp_configuration(pd_port, true);
}

void pe_dfp_vdm_dp_configuration_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_dp_configuration(pd_port, false);
}

#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */

/*
 * UVDM
 */

#ifdef CONFIG_USB_PD_CUSTOM_VDM

void pe_dfp_uvdm_send_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_send_uvdm(pd_port);
}

void pe_dfp_uvdm_acked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_uvdm(pd_port, true);
}

void pe_dfp_uvdm_naked_entry(struct pd_port *pd_port)
{
	pd_dpm_dfp_inform_uvdm(pd_port, false);
}

#endif      /* CONFIG_USB_PD_CUSTOM_VDM */
