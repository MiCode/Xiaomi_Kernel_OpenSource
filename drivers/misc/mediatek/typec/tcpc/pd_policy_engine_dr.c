// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

/*
 * [PD2.0]
 * Figure 8-53 Dual-Role (Source) Get Source Capabilities diagram
 * Figure 8-54 Dual-Role (Source) Give Sink Capabilities diagram
 * Figure 8-55 Dual-Role (Sink) Get Sink Capabilities State Diagram
 * Figure 8-56 Dual-Role (Sink) Give Source Capabilities State Diagram
 */

void pe_dr_src_get_source_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG_OR_RJ(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SOURCE_CAP);
}

void pe_dr_src_get_source_cap_exit(struct pd_port *pd_port)
{
	pd_dpm_dr_inform_source_cap(pd_port);
}

void pe_dr_src_give_sink_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_sink_caps(pd_port);
}

void pe_dr_snk_get_sink_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG_OR_RJ(pd_port);

	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SINK_CAP);
}

void pe_dr_snk_get_sink_cap_exit(struct pd_port *pd_port)
{
	pd_dpm_dr_inform_sink_cap(pd_port);
}

void pe_dr_snk_give_source_cap_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_source_caps(pd_port);
}

#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
void pe_dr_snk_give_source_cap_ext_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_TX_SUCCESS(pd_port);

	pd_dpm_send_source_cap_ext(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pe_dr_src_get_source_cap_ext_entry(struct pd_port *pd_port)
{
	PE_STATE_WAIT_MSG(pd_port);
	pd_send_sop_ctrl_msg(pd_port, PD_CTRL_GET_SOURCE_CAP_EXT);
}

void pe_dr_src_get_source_cap_ext_exit(struct pd_port *pd_port)
{
	pd_dpm_inform_source_cap_ext(pd_port);
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */
