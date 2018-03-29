/*
 * Copyright (C) 2014 mtk
 *
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __LINUX_XHCI_MTK_H
#define __LINUX_XHCI_MTK_H

#include "xhci.h"

#define MTK_SCH_NEW	1

#define SCH_SUCCESS 1
#define SCH_FAIL	0

#define MAX_EP_NUM			64
#define SS_BW_BOUND			51000
#define HS_BW_BOUND			6144
/* #define HS_BW_BOUND                   6145    // for HS HB transfer test. (Test Plan SOP 3.4.1 and 3.4.2) */

#define USB_EP_CONTROL	0
#define USB_EP_ISOC		1
#define USB_EP_BULK		2
#define USB_EP_INT		3

#define USB_SPEED_LOW	1
#define USB_SPEED_FULL	2
#define USB_SPEED_HIGH	3
#define USB_SPEED_SUPER	5

/* mtk scheduler bitmasks */
#define BPKTS(p)	((p) & 0x3f)
#define BCSCOUNT(p)	(((p) & 0x7) << 8)
#define BBM(p)		((p) << 11)
#define BOFFSET(p)	((p) & 0x3fff)
#define BREPEAT(p)	(((p) & 0x7fff) << 16)


#if 1
typedef unsigned int mtk_u32;
typedef unsigned long long mtk_u64;
#endif

#define NULL ((void *)0)

struct mtk_xhci_ep_ctx {
	mtk_u32 ep_info;
	mtk_u32 ep_info2;
	mtk_u64 deq;
	mtk_u32 tx_info;
	/* offset 0x14 - 0x1f reserved for HC internal use */
	mtk_u32 reserved[3];
};

struct sch_ep {
	/* device info */
	int dev_speed;
	int isTT;
	/* ep info */
	int is_in;
	int ep_type;
	int maxp;
	int interval;
	int burst;
	int mult;
	/* scheduling info */
	int offset;
	int repeat;
	int pkts;
	int cs_count;
	int burst_mode;
	/* other */
	int bw_cost;		/* bandwidth cost in each repeat; including overhead */
	mtk_u32 *ep;		/* address of usb_endpoint pointer */
};

/**
 * To simplify scheduler algorithm, set a upper limit for ESIT,
 * if a synchromous ep's ESIT is larger than @XHCI_MTK_MAX_ESIT,
 * round down to the limit value, that means allocating more
 * bandwidth to it.
 */
#define XHCI_MTK_MAX_ESIT	64

/**
 * struct mu3h_sch_bw_info: schedule information for bandwidth domain
 *
 * @bus_bw: array to keep track of bandwidth already used at each uframes
 * @bw_ep_list: eps in the bandwidth domain
 *
 * treat a HS root port as a bandwidth domain, but treat a SS root port as
 * two bandwidth domains, one for IN eps and another for OUT eps.
 */
struct mu3h_sch_bw_info {
	u32 bus_bw[XHCI_MTK_MAX_ESIT];
	struct list_head bw_ep_list;
};

/**
 * struct mu3h_sch_ep_info: schedule information for endpoint
 *
 * @esit: unit is 125us, equal to 2 << Interval field in ep-context
 * @num_budget_microframes: number of continuous uframes
 *		(@repeat==1) scheduled within the interval
 * @bw_cost_per_microframe: bandwidth cost per microframe
 * @endpoint: linked into bandwidth domain which it belongs to
 * @ep: address of usb_host_endpoint struct
 * @offset: which uframe of the interval that transfer should be
 *		scheduled first time within the interval
 * @repeat: the time gap between two uframes that transfers are
 *		scheduled within a interval. in the simple algorithm, only
 *		assign 0 or 1 to it; 0 means using only one uframe in a
 *		interval, and 1 means using @num_budget_microframes
 *		continuous uframes
 * @pkts: number of packets to be transferred in the scheduled uframes
 * @cs_count: number of CS that host will trigger
 * @burst_mode: burst mode for scheduling. 0: normal burst mode,
 *		distribute the bMaxBurst+1 packets for a single burst
 *		according to @pkts and @repeat, repeate the burst multiple
 *		times; 1: distribute the (bMaxBurst+1)*(Mult+1) packets
 *		according to @pkts and @repeat. normal mode is used by
 *		default
 */
struct mu3h_sch_ep_info {
	u32 esit;
	u32 num_budget_microframes;
	u32 bw_cost_per_microframe;
	struct list_head endpoint;
	void *ep;
	/*
	 * mtk xHCI scheduling information put into reserved DWs
	 * in ep context
	 */
	u32 offset;
	u32 repeat;
	u32 pkts;
	u32 cs_count;
	u32 burst_mode;
};

#define MU3C_U3_PORT_MAX 4
#define MU3C_U2_PORT_MAX 5

/**
 * struct mu3c_ippc_regs: MTK ssusb ip port control registers
 * @ip_pw_ctr0~3: ip power and clock control registers
 * @ip_pw_sts1~2: ip power and clock status registers
 * @ip_xhci_cap: ip xHCI capability register
 * @u3_ctrl_p[x]: ip usb3 port x control register, only low 4bytes are used
 * @u2_ctrl_p[x]: ip usb2 port x control register, only low 4bytes are used
 * @u2_phy_pll: usb2 phy pll control register
 */
struct mu3c_ippc_regs {
	__le32 ip_pw_ctr0;
	__le32 ip_pw_ctr1;
	__le32 ip_pw_ctr2;
	__le32 ip_pw_ctr3;
	__le32 ip_pw_sts1;
	__le32 ip_pw_sts2;
	__le32 reserved0[3];
	__le32 ip_xhci_cap;
	__le32 reserved1[2];
	__le64 u3_ctrl_p[MU3C_U3_PORT_MAX];
	__le64 u2_ctrl_p[MU3C_U2_PORT_MAX];
	__le32 reserved2;
	__le32 u2_phy_pll;
	__le32 reserved3[33]; /* 0x80 ~ 0xff */
};

struct xhci_hcd_mtk {
	struct device *dev;
	struct usb_hcd *hcd;
	struct mu3h_sch_bw_info *sch_array;
	struct mu3c_ippc_regs __iomem *ippc_regs;
	int num_u2_ports;
	int num_u3_ports;
	struct regulator *vusb33;
	struct regulator *vbus;
	struct clk *sys_clk;	/* sys and mac clock */
	struct clk *wk_deb_p0;	/* port0's wakeup debounce clock */
	struct clk *wk_deb_p1;
	struct regmap *pericfg;
	struct phy **phys;
	int num_phys;
	int wakeup_src;
	bool lpm_support;
};

static inline struct xhci_hcd_mtk *hcd_to_mtk(struct usb_hcd *hcd)
{
	return dev_get_drvdata(hcd->self.controller);
}
int xhci_mtk_sch_init(struct xhci_hcd_mtk *mtk);
void xhci_mtk_sch_exit(struct xhci_hcd_mtk *mtk);
int xhci_mtk_add_ep_quirk(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep);
void xhci_mtk_drop_ep_quirk(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep);

extern int mtk_xhci_scheduler_init(void);
extern int mtk_xhci_scheduler_add_ep(int dev_speed, int is_in, int isTT, int ep_type, int maxp,
			      int interval, int burst, int mult, mtk_u32 *ep, mtk_u32 *ep_ctx,
			      struct sch_ep *sch_ep);
extern struct sch_ep *mtk_xhci_scheduler_remove_ep(int dev_speed, int is_in, int isTT, int ep_type,
					    mtk_u32 *ep);
extern int mtk_xhci_ip_init(struct usb_hcd *hcd, struct xhci_hcd *xhci);
extern void mtk_xhci_reset(struct xhci_hcd *xhci);

extern void mtk_xhci_vbus_on(struct platform_device *pdev);
extern void mtk_xhci_vbus_off(struct platform_device *pdev);
extern int get_num_u3_ports(struct xhci_hcd *xhci);
extern int get_num_u2_ports(struct xhci_hcd *xhci);

#define XHCI_DRIVER_NAME		"xhci"
#define XHCI_BASE_REGS_ADDR_RES_NAME	"ssusb_base"
#define XHCI_SIF_REGS_ADDR_RES_NAME	"ssusb_sif"
#define XHCI_SIF2_REGS_ADDR_RES_NAME	"ssusb_sif2"

#endif /* __LINUX_XHCI_MTK_H */
