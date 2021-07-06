// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */
#ifndef __PMIF_H__
#define __PMIF_H__

#include <linux/clk.h>

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
 * @toprgu_base:	toprgu base address.
 * @toprgu_regs:	toprgu register offset table.
 * @dbgregs:	pmif debug register offset table.
 * @dbgver:	pmif debug version.
 * @swinf_ch_start:	indicate sw channel start number, lower is hw channel.
 * @ap_swinf_no:	indicate ap sw channel number.
 * @write:	indicate write cmd.
 * @mstid:	indicate mstid.
 * @pmifid:	indicate pmifid.
 * @irq:	indicate irq number.
 * @grpid:	indicates which group id we used.
 * @lock:	indicate lock key.
 * @pmifThread_lock:	indicate pmif interrupt thread pm protection.
 * @pmif_mutex:	indicate pmif interrupt thread mutex protection.
 * @spmic:	indicate spmi controller.
 * @pmif_sys_ck:	indicate pmif infracfg_ao sys_ck cg.
 * @pmif_tmr_ck:	indicate pmif infracfg_ao tmr_ck cg.
 * @pmif_clk_mux:	indicate pmif clock source, be as consumer.
 * @pmif_clk_osc_d10:	indicate pmif clock source to osc d10.
 * @pmif_clk26m:	indicate pmif clock source to clk26m.
 * @spmimst_clk_mux:	indicate spmimst clock source, be as consumer.
 * @spmimst_clk26m:	indicate spmimst clock sourc to clk26m.
 * @spmimst_clk_osc_d10:	indicate spmimst clock source to osc d10.
 * @cmd:	sends a non-data command sequence on the SPMI bus.
 * @read_cmd:	sends a register read command sequence on the SPMI bus.
 * @write_cmd:	sends a register write command sequence on the SPMI bus.
 * @pmif_enable_clk_set:	pmif clock set.
 * @pmif_force_normal_mode:	enable pmif normal mode.
 * @pmif_enable_swinf:	enable sw interface arbritation.
 * @pmif_enable_cmdIssue:	enable cmd enable to access.
 * @pmif_enable:	set pmif all done.
 * @is_pmif_init_done:	check if pmif init done.
 * @pmif_enable_reset:	SW reset pmif.
 * @pmif_cali_clock:	calibrate spmi master clock.
 * @spmi_config_master:	config spmi master.
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
	void __iomem		*toprgu_base;
	const u32               *toprgu_regs;
	const u32		*dbgregs;
	u32			dbgver;
	u32                     swinf_ch_start;
	u32                     ap_swinf_no;
	int                     write;
	int                     mstid;
	int                     pmifid;
	int			irq;
	int			grpid;
	raw_spinlock_t          lock;
	struct wakeup_source *pmifThread_lock;
	struct mutex pmif_mutex;
	struct spmi_controller  *spmic;
	struct clk *pmif_sys_ck;
	struct clk *pmif_tmr_ck;
	struct clk *pmif_clk_mux;
	struct clk *pmif_clk_osc_d10;
	struct clk *pmif_clk26m;
	struct clk *spmimst_clk_mux;
	struct clk *spmimst_clk26m;
	struct clk *spmimst_clk_osc_d10;
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
	void (*pmif_enable_reset)(struct pmif *arb);
	int (*pmif_cali_clock)(struct pmif *arb);
	int (*spmi_config_master)(struct pmif *arb, unsigned int mstid,
			bool en);
};
#endif /*__PMIF_H__*/
