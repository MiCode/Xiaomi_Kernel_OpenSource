/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DWC3_DEBUG_IPC_H
#define __DWC3_DEBUG_IPC_H

#include "core.h"
#include "debug.h"
#include <linux/ipc_logging.h>

/*
 * NOTE: Make sure to have mdwc as local variable in function before using
 * below macros.
 */

#define USB3_PRI_IPCAT_REG	0x0
#define USB3_PRI_CTRL_REG	0x4
#define USB3_PRI_GENERAL_CFG	0x8
#define USB3_PRI_RAM1_REG	0xC
#define USB3_PRI_HS_PHY_CTRL	0x10
#define USB3_PRI_CHARGING_DET_OUTPUT	0x1C
#define USB3_PRI_ALT_INTERRUPT_EN	0x20
#define USB3_PRI_HS_PHY_IRQ_STAT	0x24
#define USB3_PRI_CGCTL_REG	0x28
#define USB3_PRI_DBG_BUS_REG	0x2C
#define USB3_PRI_SS_PHY_CTRL	0x30
#define USB3_PRI_DBG_BUS_DATA	0x34
#define USB3_PRI_PWR_EVNT_IRQ_STAT	0x58
#define USB3_PRI_PWR_EVNT_IRQ_MASK	0x5C
#define USB3_PRI_HW_SW_EVT_CTRL_REG	0x60
#define USB3_PRI_FLADJ_30MHZ_REG	0x68
#define USB3_PRI_M_AW_USER_REG	0x6C
#define USB3_PRI_M_AR_USER_REG	0x70
#define USB3_PRI_QSCRTCH_REG_n	0xB4
#define USB3_PRI_SS_QMP_PHY_CTRL	0xF0
#define USB3_PRI_SNPS_CORE_CFG	0xF4
#define USB3_PRI_USB30_STS_REG	0xF8
#define USB3_PRI_USB30_GSI_GENERAL_CFG	0xFC
#define USB3_PRI_USB30_GSI_EVT_POINTER_L	0x100
#define USB3_PRI_USB30_GSI_EVT_POINTER_H	0x104
#define USB3_PRI_USB30_GSI_EVT_ON_ERR_L	0x108
#define USB3_PRI_USB30_GSI_EVT_ON_ERR_H	0x10C
#define USB3_PRI_USB30_GSI_DBL_ADDR_Ln	0x110
#define USB3_PRI_USB30_GSI_DBL_ADDR_Hn	0x120
#define USB3_PRI_USB30_GSI_RING_BASE_ADDR_Ln	0x130
#define USB3_PRI_USB30_GSI_RING_BASE_ADDR_Hn	0x144
#define USB3_PRI_USB30_GSI_DEPCMD_ADDR_L_IPA_EPn	0x150
#define USB3_PRI_USB30_IMODn	0x170
#define USB3_PRI_USB30_USEC_CNT	0x180
#define USB3_PRI_USB30_GSI_IF_CAP_WB_ADDRn	0x184
#define USB3_PRI_USB30_GSI_IF_CAP_WB_ADDRn_H	0x190
#define USB3_PRI_USB30_GSI_IF_STS1	0x1A4
#define USB3_PRI_USB30_GSI_EVT_ON_ERR2_L	0x1A8
#define USB3_PRI_USB30_GSI_EVT_ON_ERR2_H	0x1AC
#define USB3_PRI_USB30_DWC_AWQOS	0x1B0
#define USB3_PRI_USB30_DWC_ARQOS	0x1B4
#define USB3_PRI_USB30_DWC_AWQOSARB	0x1B8
#define USB3_PRI_USB30_DWC_ARQOSARB	0x1BC
#define USB3_PRI_USB30_DWC_EXTRA_INPUT_0	0x1C4
#define USB3_PRI_USB30_LTSSM_ST_TRANS_EVENT_REG	0x1F4
#define USB3_PRI_USB30_BC_CTRL_MUX_MASK_ADDR	0x1F8
#define USB3_PRI_USB30_CHIKEN_BIT_REG	0x1FC
#define USB3_PRI_USB30_LPC_SCAN_MASK	0x200
#define USB3_PRI_USB30_LPC_REG	0x204
#define USB3_PRI_USB30_PIPE3_CLK_REG	0x208
#define USB3_PRI_USB30_OVRD_DEV_SPEED	0x20C
#define USB3_PRI_USB30_MODE_SEL	0x210
#define USB3_PRI_USB30_QDSS_CONFIG	0x214

#define dump_qscratch_regs(nm)				\
{							\
	.name	= __stringify(nm),			\
	.offset	= USB3_##nm,				\
}

static struct debugfs_reg32 qscratch_reg[] = {
	dump_qscratch_regs(PRI_IPCAT_REG),
	dump_qscratch_regs(PRI_CTRL_REG),
	dump_qscratch_regs(PRI_GENERAL_CFG),
	dump_qscratch_regs(PRI_RAM1_REG),
	dump_qscratch_regs(PRI_HS_PHY_CTRL),
	dump_qscratch_regs(PRI_CHARGING_DET_OUTPUT),
	dump_qscratch_regs(PRI_ALT_INTERRUPT_EN),
	dump_qscratch_regs(PRI_HS_PHY_IRQ_STAT),
	dump_qscratch_regs(PRI_CGCTL_REG),
	dump_qscratch_regs(PRI_DBG_BUS_REG),
	dump_qscratch_regs(PRI_SS_PHY_CTRL),
	dump_qscratch_regs(PRI_DBG_BUS_DATA),
	dump_qscratch_regs(PRI_PWR_EVNT_IRQ_STAT),
	dump_qscratch_regs(PRI_PWR_EVNT_IRQ_MASK),
	dump_qscratch_regs(PRI_HW_SW_EVT_CTRL_REG),
	dump_qscratch_regs(PRI_FLADJ_30MHZ_REG),
	dump_qscratch_regs(PRI_M_AW_USER_REG),
	dump_qscratch_regs(PRI_M_AR_USER_REG),
	dump_qscratch_regs(PRI_QSCRTCH_REG_n),
	dump_qscratch_regs(PRI_SS_QMP_PHY_CTRL),
	dump_qscratch_regs(PRI_SNPS_CORE_CFG),
	dump_qscratch_regs(PRI_USB30_STS_REG),
	dump_qscratch_regs(PRI_USB30_GSI_GENERAL_CFG),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_POINTER_L),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_POINTER_H),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_ON_ERR_L),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_ON_ERR_H),
	dump_qscratch_regs(PRI_USB30_GSI_DBL_ADDR_Ln),
	dump_qscratch_regs(PRI_USB30_GSI_DBL_ADDR_Hn),
	dump_qscratch_regs(PRI_USB30_GSI_RING_BASE_ADDR_Ln),
	dump_qscratch_regs(PRI_USB30_GSI_RING_BASE_ADDR_Hn),
	dump_qscratch_regs(PRI_USB30_GSI_DEPCMD_ADDR_L_IPA_EPn),
	dump_qscratch_regs(PRI_USB30_IMODn),
	dump_qscratch_regs(PRI_USB30_USEC_CNT),
	dump_qscratch_regs(PRI_USB30_GSI_IF_CAP_WB_ADDRn),
	dump_qscratch_regs(PRI_USB30_GSI_IF_CAP_WB_ADDRn_H),
	dump_qscratch_regs(PRI_USB30_GSI_IF_STS1),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_ON_ERR2_L),
	dump_qscratch_regs(PRI_USB30_GSI_EVT_ON_ERR2_H),
	dump_qscratch_regs(PRI_USB30_DWC_AWQOS),
	dump_qscratch_regs(PRI_USB30_DWC_ARQOS),
	dump_qscratch_regs(PRI_USB30_DWC_AWQOSARB),
	dump_qscratch_regs(PRI_USB30_DWC_ARQOSARB),
	dump_qscratch_regs(PRI_USB30_DWC_EXTRA_INPUT_0),
	dump_qscratch_regs(PRI_USB30_LTSSM_ST_TRANS_EVENT_REG),
	dump_qscratch_regs(PRI_USB30_BC_CTRL_MUX_MASK_ADDR),
	dump_qscratch_regs(PRI_USB30_CHIKEN_BIT_REG),
	dump_qscratch_regs(PRI_USB30_LPC_SCAN_MASK),
	dump_qscratch_regs(PRI_USB30_LPC_REG),
	dump_qscratch_regs(PRI_USB30_PIPE3_CLK_REG),
	dump_qscratch_regs(PRI_USB30_OVRD_DEV_SPEED),
	dump_qscratch_regs(PRI_USB30_MODE_SEL),
	dump_qscratch_regs(PRI_USB30_QDSS_CONFIG),
};

#define dbg_event(ep_num, name, status) \
	dwc3_dbg_print(mdwc->dwc_ipc_log_ctxt, ep_num, name, status, "")

#define dbg_print(ep_num, name, status, extra) \
	dwc3_dbg_print(mdwc->dwc_ipc_log_ctxt, ep_num, name, status, extra)

#define dbg_print_reg(name, reg) \
	dwc3_dbg_print_reg(mdwc->dwc_ipc_log_ctxt, name, reg)

#define dbg_done(ep_num, count, status) \
	dwc3_dbg_done(mdwc->dwc_ipc_log_ctxt, ep_num, count, status)

#define dbg_queue(ep_num, req, status) \
	dwc3_dbg_queue(mdwc->dwc_ipc_log_ctxt, ep_num, req, status)

#define dbg_setup(ep_num, req) \
	dwc3_dbg_setup(mdwc->dwc_ipc_log_ctxt, ep_num, req)

#define dbg_ep_queue(ep_num, req) \
	dwc3_dbg_dma_queue(mdwc->dwc_ipc_log_ctxt, ep_num, req)

#define dbg_ep_dequeue(ep_num, req) \
	dwc3_dbg_dma_dequeue(mdwc->dwc_ipc_log_ctxt, ep_num, req)

#define dbg_ep_unmap(ep_num, req) \
	dwc3_dbg_dma_unmap(mdwc->dwc_dma_ipc_log_ctxt, ep_num, req)

#define dbg_ep_map(ep_num, req) \
	dwc3_dbg_dma_map(mdwc->dwc_dma_ipc_log_ctxt, ep_num, req)

#define dump_dwc3_regs(name, offset, value) \
	dbg_dwc3_dump_regs(mdwc->dwc_dma_ipc_log_ctxt, name, offset, value)

#define dbg_log_string(fmt, ...) \
	ipc_log_string(mdwc->dwc_ipc_log_ctxt,\
			"%s: " fmt, __func__, ##__VA_ARGS__)

#define dbg_trace_ctrl_req(ctrl) \
	dwc3_dbg_trace_log_ctrl(dwc_trace_ipc_log_ctxt, ctrl)

#define dbg_trace_ep_queue(req) \
	dwc3_dbg_trace_log_request(dwc_trace_ipc_log_ctxt, req, "dbg_ep_queue")

#define dbg_trace_ep_dequeue(req) \
	dwc3_dbg_trace_log_request(dwc_trace_ipc_log_ctxt, req, "dbg_ep_dequeue")

#define dbg_trace_gadget_giveback(req) \
	dwc3_dbg_trace_log_request(dwc_trace_ipc_log_ctxt, req, "dbg_gadget_giveback")

#define dbg_trace_gadget_ep_cmd(dep, cmd, params, cmd_status) \
	dwc3_dbg_trace_ep_cmd(dwc_trace_ipc_log_ctxt, dep, cmd, params, cmd_status)

#define dbg_trace_trb_prepare(dep, event) \
	dwc3_dbg_trace_trb_complete(dwc_trace_ipc_log_ctxt, dep, trb, "dbg_prepare")

#define dbg_trace_trb_complete(dep, event) \
	dwc3_dbg_trace_trb_complete(dwc_trace_ipc_log_ctxt, dep, trb, "dbg_complete")

#define dbg_trace_event(event, dwc) \
	dwc3_dbg_trace_event(dwc_trace_ipc_log_ctxt, event, dwc)

void dwc3_dbg_trace_log_ctrl(void *log_ctxt, struct usb_ctrlrequest *ctrl);
void dwc3_dbg_trace_log_request(void *log_ctxt, struct dwc3_request *req,
				char *tag);
void dwc3_dbg_trace_ep_cmd(void *log_ctxt, struct dwc3_ep *dep,
				unsigned int cmd,
				struct dwc3_gadget_ep_cmd_params *params,
				int cmd_status);
void dwc3_dbg_trace_trb_complete(void *log_ctxt, struct dwc3_ep *dep,
					struct dwc3_trb *trb, char *tag);
void dwc3_dbg_trace_event(void *log_ctxt, u32 event, struct dwc3 *dwc);
void dwc3_dbg_print(void *log_ctxt, u8 ep_num,
		const char *name, int status, const char *extra);
void dwc3_dbg_done(void *log_ctxt, u8 ep_num,
		const u32 count, int status);
void dwc3_dbg_event(void *log_ctxt, u8 ep_num,
		const char *name, int status);
void dwc3_dbg_queue(void *log_ctxt, u8 ep_num,
		const struct usb_request *req, int status);
void dwc3_dbg_setup(void *log_ctxt, u8 ep_num,
		const struct usb_ctrlrequest *req);
void dwc3_dbg_print_reg(void *log_ctxt,
		const char *name, int reg);
void dwc3_dbg_dma_queue(void *log_ctxt, u8 ep_num,
			struct dwc3_request *req);
void dwc3_dbg_dma_dequeue(void *log_ctxt, u8 ep_num,
			struct dwc3_request *req);
void dwc3_dbg_dma_map(void *log_ctxt, u8 ep_num,
			struct dwc3_request *req);
void dwc3_dbg_dma_unmap(void *log_ctxt, u8 ep_num,
			struct dwc3_request *req);
void dbg_dwc3_dump_regs(void *log_ctxt, char *name, int offset, int value);

#endif /* __DWC3_DEBUG_IPC_H */
