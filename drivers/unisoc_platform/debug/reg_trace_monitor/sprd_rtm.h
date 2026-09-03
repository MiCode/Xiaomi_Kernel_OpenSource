/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *copyright (C) 2024 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_RTM_H
#define _SPRD_RTM_H

/* RTM register */
#define REG_TRACE_CTRL		0x0
#define FRAME_END_CTRL		0x4
#define OVERFLOW_EVT_CLR	0X8
#define OVERFLOW_EVT_STS	0XC
#define TS_FLD_SEL			0X10
#define APB_PORTS_PARAM		0X14
#define AHB_PORTS_PARAM		0x18
#define AXI_PORTS_PARAM		0x1C
#define ATB_ID				0X20
#define AXI_WAIT_THRESHOLD	0X24
#define IP_VERSION			0X28
#define FIFO_EMPTY_STS		0X2C
#define REMAIN_DATA_STS		0X30
#define FRAME_BUFFER_BITS	0X34

/* BUS CONTROL SEL */
#define APB_PORT_CTLR(i)	(0X40+(0X4*i))
#define APB_PORT_FILTER(i)	(0X44+(0X4*i))
#define AHB_PORT_CTLR(i)	(0X80+(0X4*i))
#define AHB_PORT_FILTER(i)	(0X84+(0X4*i))
#define AXI_PORT_CTLR(i)	(0XC0+(0X4*i))
#define AXI_PORT_FILTER(i)	(0XC4+(0X4*i))

/* REG_TRACE_CTLR REGISTER DEFINATION */
#define RTM_EN				BIT(0)
#define FRM_TS_END_EN		BIT(1)
#define APB_USER_FLD_EN		BIT(2)
#define AHB_USER_FLD_EN		BIT(3)
#define AXI_USER_FLD_EN		BIT(4)
#define BUS_USER_FLD_EN		(APB_USER_FLD_EN | AHB_USER_FLD_EN | AXI_USER_FLD_EN)
#define APB_ADDR_FLD_EN		BIT(5)
#define AHB_ADDR_FLD_EN		BIT(6)
#define AXI_ADDR_FLD_EN		BIT(7)
#define BUS_ADDR_FLD_EN		(APB_ADDR_FLD_EN | AHB_ADDR_FLD_EN | AXI_ADDR_FLD_EN)

/* FRAME_END_CTRL REGISTER DEFINATION */
#define PKG_NUMS_PER_FRM_MASK		0x3F
#define TS_THRESHOLD_PER_FRM_MASK	0xFFFF0000

/* TS_FLD_SEL REGISTER DEFINATION */
#define TS_FLD_MASK		0XF

#define ATB_ID_MASK		0x7F
#define RTM_VERSION		0X100
#define PORT_NUM_MAX	24

/* APB/AHB/AXI_PORT_CTLR REGISTER DEFINATION */
#define	TARCE_ID_TYPE			BIT(1)
#define TARCE_DIRECTION_SEL		BIT(0)
#define	TRACE_NONE				0x0
#define TRACE_WRITE				0x1
#define TRACE_READ				0x2
#define TRACE_WR_RD				0x3
#define TARCE_DIRECTION_MASK	0x3
#define TRACE_FILTER_USER		0x0
#define TRACE_FILTER_ID			0x1
#define RTM_CONFIG_ELEMENT_SIZE 5

#define BUS_TYPE_PRINT(bus)			(bus ? ((bus == 1) ? "AHB" : \
						"AXI") : "APB")
#define BUS_DIRECTION_PRINT(dir)	(dir ? ((dir == 1) ? "WO" : \
						((dir == 2) ? "RO" : "RW")) : "NO")

/* APB/AHB/AXI_PORT_FILTER REGISTER DEFINATION */
#define FILTER_USER_MASK		0x7FFF
#define FILTER_USER_MATH		0x7FFF

// FUNNEL REGISTER
#define RTM_FUNNEL_FUNCTL			0x000
#define RTM_FUNNEL_PRICTL			0x004
#define RTM_FUNNEL_HOLDTIME_MASK	0xf00
#define RTM_FUNNEL_HOLDTIME			(0x7<<0x8)

//REPLICATION REGISTER
#define RTM_REPLICATOR_IDFILTER0		0x000
#define RTM_REPLICATOR_IDFILTER1		0x004

// TMC REGISTER
#define RTM_TMC_RSZ			0x004
#define RTM_TMC_CTL			0x020
#define RTM_TMC_MODE		0x028
#define RTM_TMC_DBALO		0x118
#define RTM_TMC_DBAHI		0x11c
#define LOCK_OFFSET		0xFB0
#define LOCK_ACCESS		0xC5ACCE55
enum rtm_port_type {
	APB_PORT_TYPE,
	AHB_PORT_TYPE,
	AXI_PORT_TYPE,
};

enum rtm_trace_en {
	RTM_TRACE_STOP,
	RTM_TRACE_START,
};

enum rtm_trace_sink {
	RTM_TO_TMC_ETF,
	RTM_TO_TMC_ETR,
};

enum rtm_tmc_mode {
	TMC_CIRCULAR_BUFFER,
	TMC_SOFTWARE_FIFO,
	TMC_HARDWARE_FIFO,
};

struct rtm_port_cfg {
	const u32 port_id;
	const u32 bus_type;
	u32 direction;
	u32 filter_type;
	u32 match_num;
};

struct rtm_glb_cfg {
	bool frm_ts_end_en;
	bool uesr_ext_en;
	bool addr_ext_en;
	u8  pkg_per_frame;
	u32 ts_threshold;
	u8  ts_fld_sel;
	u8  atb_id;
	u16 axi_wait_threshold;
	u16 version;
};

struct rtm_atb_clk {
	struct regmap *aon_apb_base;
	u32 offset;
	u32 mask;
	u32 apb_clk_mask;
};

struct rtm_trace_cfg {
	void __iomem		*funnel_base;
	void __iomem		*etf_base;
	void __iomem		*replicate_base;
	void __iomem		*etr_base;
	enum rtm_trace_sink sink;
	u8  funnel_port;
	u64 hwaddr;
};

struct sprd_rtm_device {
	enum rtm_trace_en		enable;
	u8						port_num;
	struct rtm_glb_cfg		glb_cfg;
	const char				**port_name;
	struct rtm_port_cfg		*port_cfg;
	struct rtm_trace_cfg	trace_cfg;
	const struct rtm_ops	*ops;
};

struct sprd_rtm_drvdata {
	struct device		*dev;
	struct miscdevice	misc;
	void __iomem		*base;
	struct rtm_atb_clk	atb_clk;
	struct clk			*cs_clk;
	struct clk			*cs_src_sel;
	struct sprd_rtm_device	*rtm_dev;
};

struct rtm_ops {
	void (*trace_en)(struct sprd_rtm_drvdata *rtm, bool enable);
	void (*frm_ts_en)(struct sprd_rtm_drvdata *rtm, bool enable);
	void (*user_en)(struct sprd_rtm_drvdata *rtm, bool enable);
	void (*addr_en)(struct sprd_rtm_drvdata *rtm, bool enable);
	void (*pkg_frm)(struct sprd_rtm_drvdata *rtm, u8 frame);
	void (*ts_threshold)(struct sprd_rtm_drvdata *rtm, u32 thsld);
	void (*ts_fld_sel)(struct sprd_rtm_drvdata *rtm, u8 region);
	void (*atb_id)(struct sprd_rtm_drvdata *rtm, u8 id);
	void (*axi_wait_threshold)(struct sprd_rtm_drvdata *rtm, u16 thsld);
	void (*version)(struct sprd_rtm_drvdata *rtm, u16 *ver);
};
#endif
