// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC

void pe_dbg_ready_entry(struct pd_port *pd_port)
{
	uint8_t state;

	if (pd_port->pe_data.pe_ready)
		return;

	pd_port->pe_data.pe_ready = true;
	pd_reset_protocol_layer(pd_port, false);

	if (pd_port->data_role == PD_ROLE_UFP) {
		PE_INFO("Custom_DBGACC : UFP\n");
		state = PD_CONNECT_PE_READY_DBGACC_UFP;
		pd_set_rx_enable(pd_port, PD_RX_CAP_PE_READY_UFP);
	} else {
		PE_INFO("Custom_DBGACC : DFP\n");
		state = PD_CONNECT_PE_READY_DBGACC_DFP;
		pd_set_rx_enable(pd_port, PD_RX_CAP_PE_READY_DFP);
	}

	pd_update_connect_state(pd_port, state);
}

#endif /* CONFIG_USB_PD_CUSTOM_DBGACC */
