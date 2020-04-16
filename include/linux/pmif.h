// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */
#ifndef __PMIF_H__
#define __PMIF_H__

enum {
	SPMI_MASTER_0 = 0,
	SPMI_MASTER_1,
	SPMI_MASTER_2,
	SPMI_MASTER_3
};
/**
 * @base:	pmif base address.
 * @regs:	pmif register offset table.
 * @spmimst_base:	spmi master base address.
 * @spmimst_regs:	spmi master register offset table.
 * @infra_base:	infra base address.
 * @infra_regs:	infra register offset table.
 * @topckgen_base:	topckgen base address.
 * @topckgen_regs:	topckgen register offset table.
 * @swinf_ch_start:	indicate sw channel start number, lower is hw channel.
 * @ap_swinf_no:	indicate ap sw channel number.
 * @write:	indicate write cmd.
 * @pmifid:	indicate pmifid.
 * @irq:	indicate irq number.
 * @grpiden:	bitmask indicates which group id we used.
 * @lock:	indicate lock key.
 * @spmic:	indicate spmi controller.
 * @cmd:	sends a non-data command sequence on the SPMI bus.
 * @read_cmd:	sends a register read command sequence on the SPMI bus.
 * @write_cmd:	sends a register write command sequence on the SPMI bus.
 * @pmif_enable_clk_set:	pmif clock set.
 * @pmif_force_normal_mode:	enable pmif normal mode.
 * @pmif_enable_swinf:	enable sw interface arbritation.
 * @pmif_enable_cmdIssue:	enable cmd enable to access.
 * @pmif_enable:	set pmif all done.
 * @is_pmif_init_done:	check if pmif init done.
 */
struct pmif {
	void __iomem		*base;
	const u32		*regs;
	void __iomem		*spmimst_base;
	const u32		*spmimst_regs;
	void __iomem		*infra_base;
	const u32               *infra_regs;
	void __iomem		*topckgen_base;
	const u32               *topckgen_regs;
	u32                     swinf_ch_start;
	u32                     ap_swinf_no;
	int                     write;
	int                     pmifid;
	int			irq;
	int			grpiden;
	raw_spinlock_t          lock;
	struct spmi_controller  *spmic;
	int (*cmd)(struct spmi_controller *ctrl, unsigned int opcode);
	int (*read_cmd)(struct spmi_controller *ctrl, u8 opc, u8 sid,
			u16 addr, u8 *buf, size_t len);
	int (*write_cmd)(struct spmi_controller *ctrl, u8 opc, u8 sid,
			u16 addr, const u8 *buf, size_t len);
	void (*pmif_enable_clk_set)(struct pmif *arb);
	void (*pmif_force_normal_mode)(struct pmif *arb);
	void (*pmif_enable_swinf)(struct pmif *arb, unsigned int chan_no,
			unsigned int swinf_no);
	void (*pmif_enable_cmdIssue)(struct pmif *arb, bool en);
	void (*pmif_enable)(struct pmif *arb);
	int (*is_pmif_init_done)(struct pmif *arb);
};

extern int mtk_spmi_enable_group_id(struct spmi_controller *ctrl,
			unsigned int grpiden);
#endif /*__PMIF_H__*/
