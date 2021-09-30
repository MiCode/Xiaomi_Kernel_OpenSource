/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */
#ifndef __SPMI_MTK_H__
#define __SPMI_MTK_H__

#include <linux/spmi.h>

#define DEFAULT_VALUE_READ_TEST		(0x5a)
#define DEFAULT_VALUE_WRITE_TEST	(0xa5)
#define SPMI_RCS_SUPPORT 0

struct ch_reg {
	u32 ch_sta;
	u32 wdata;
	u32 rdata;
	u32 ch_send;
	u32 ch_rdy;
};

struct pmif {
	void __iomem	*base;
	const u32	*regs;
	void __iomem	*spmimst_base;
	const u32	*spmimst_regs;
	const u32	*dbgregs;
	u32		dbgver;
	u32		soc_chan;
	int     mstid;
	int     pmifid;
	int		irq;
	int		grpid;
	raw_spinlock_t	lock;
	struct wakeup_source *pmifThread_lock;
	struct mutex pmif_mutex;
	struct spmi_controller  *spmic;
	struct clk	*pmif_sys_ck;
	struct clk	*pmif_tmr_ck;
	struct clk	*spmimst_clk_mux;
	struct ch_reg chan;
	struct clk *pmif_clk_mux;
	struct clk *spmimst_m_clk_mux;
	struct clk *spmimst_p_clk_mux;
	struct clk *vlp_pmif_clk_mux;
	struct clk *vlp_spmimst_m_clk_mux;
	struct clk *vlp_spmimst_p_clk_mux;
	struct irq_domain	*domain;
	struct irq_chip		irq_chip;
	int			rcs_irq;
	struct mutex		rcs_irqlock;
	bool	   *rcs_enable_hwirq;
	struct clk *spmimst_clk26m;
	struct clk *spmimst_clk_osc_d10;
	int (*cmd)(struct spmi_controller *ctrl, unsigned int opcode);
	int (*read_cmd)(struct spmi_controller *ctrl, u8 opc, u8 sid,
			u16 addr, u8 *buf, size_t len);
	int (*write_cmd)(struct spmi_controller *ctrl, u8 opc, u8 sid,
			u16 addr, const u8 *buf, size_t len);
	u32 caps;
};

enum spmi_regs {
	SPMI_OP_ST_CTRL,
	SPMI_GRP_ID_EN,
	SPMI_OP_ST_STA,
	SPMI_MST_SAMPL,
	SPMI_MST_REQ_EN,
	SPMI_REC_CTRL,
	SPMI_REC0,
	SPMI_REC1,
	SPMI_REC2,
	SPMI_REC3,
	SPMI_REC4,
	SPMI_MST_DBG,
	/* RCS support */
	SPMI_MST_RCS_CTRL,
	SPMI_MST_IRQ,
	SPMI_SLV_3_0_EINT,
	SPMI_SLV_7_4_EINT,
	SPMI_SLV_B_8_EINT,
	SPMI_SLV_F_C_EINT,
	SPMI_TIA,
	SPMI_DEC_DBG,
	SPMI_REC_CMD_DEC,
};

/* pmif debug API declaration */
extern void spmi_dump_wdt_reg(void);
extern void spmi_dump_pmif_acc_vio_reg(void);
extern void spmi_dump_pmic_acc_vio_reg(void);
extern void spmi_dump_pmif_busy_reg(void);
extern void spmi_dump_pmif_swinf_reg(void);
extern void spmi_dump_pmif_all_reg(void);
extern void spmi_dump_pmif_record_reg(void);
/* spmi debug API declaration */
extern void spmi_dump_spmimst_all_reg(void);
/* pmic debug API declaration */
extern int spmi_pmif_create_attr(struct device_driver *driver);
extern int spmi_pmif_dbg_init(struct spmi_controller *ctrl);
#endif /*__SPMI_MTK_H__*/

