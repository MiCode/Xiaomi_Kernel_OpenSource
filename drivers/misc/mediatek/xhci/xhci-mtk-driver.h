/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _XHCI_MTK_H
#define _XHCI_MTK_H

#include <linux/usb.h>
#include "mtk-phy.h"

#define _SSUSB_U3_MAC_BASE(mac_base)	(mac_base + (unsigned long)0x2400)
#define SSUSB_U3_MAC_BASE				_SSUSB_U3_MAC_BASE(mtk_xhci->base_regs)

#define _SSUSB_U3_SYS_BASE(mac_base)	(mac_base + (unsigned long)0x2600)
#define SSUSB_U3_SYS_BASE				_SSUSB_U3_SYS_BASE(mtk_xhci->base_regs)

#define _SSUSB_U2_SYS_BASE(mac_base)	(mac_base + (unsigned long)0x3400)
#define SSUSB_U2_SYS_BASE				_SSUSB_U2_SYS_BASE(mtk_xhci->base_regs)
				/* ref doc ssusb_xHCI_exclude_port_csr.xlsx */

#define _SSUSB_XHCI_EXCLUDE_BASE(mac_base)	(mac_base + 0x900)
#define SSUSB_XHCI_EXCLUDE_BASE			_SSUSB_XHCI_EXCLUDE_BASE(mtk_xhci->base_regs)

#define SIFSLV_IPPC_OFFSET					0x700

#define _U3_PIPE_LATCH_SEL_ADD(mac_base)	(_SSUSB_U3_MAC_BASE(mac_base) + (unsigned long)0x130)
#define U3_PIPE_LATCH_SEL_ADD				_U3_PIPE_LATCH_SEL_ADD(mtk_xhci->base_regs)
#define U3_PIPE_LATCH_TX	0
#define U3_PIPE_LATCH_RX	0

#define U3_UX_EXIT_LFPS_TIMING_PAR		0xa0
#define U3_REF_CK_PAR					0xb0
#define U3_RX_UX_EXIT_LFPS_REF_OFFSET	8
#define U3_RX_UX_EXIT_LFPS_REF			3
#define	U3_REF_CK_VAL					10

#define U3_TIMING_PULSE_CTRL			0xb4
/* 62.5MHz:63, 70MHz:70, 80MHz:80, 100MHz:100, 125MHz:125 */
#define MTK_CNT_1US_VALUE				63

#define USB20_TIMING_PARAMETER			0x40
/* 62.5MHz:63, 80MHz:80, 100MHz:100, 125MHz:125 */
#define MTK_TIME_VALUE_1US				63

#define LINK_PM_TIMER					0x8
#define MTK_PM_LC_TIMEOUT_VALUE			3


#define _SSUSB_IP_PW_CTRL(sif_base)		(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x0))
#define SSUSB_IP_PW_CTRL				_SSUSB_IP_PW_CTRL(mtk_xhci->sif_regs)
/* #define SSUSB_IP_SW_RST               (1<<0) */

#define _SSUSB_IP_PW_CTRL_1(sif_base)	(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x4))
#define SSUSB_IP_PW_CTRL_1				_SSUSB_IP_PW_CTRL_1(mtk_xhci->sif_regs)
#define SSUSB_IP_PDN					(1<<0)

#define _SSUSB_IP_PW_STS1(sif_base)		(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x10))
#define SSUSB_IP_PW_STS1				_SSUSB_IP_PW_STS1(mtk_xhci->sif_regs)
/* #define SSUSB_SYS125_RST_B_STS (0x1<<10) */
/* #define SSUSB_U3_MAC_RST_B_STS (0x1<<16) */

#define _SSUSB_IP_PW_STS2(sif_base)    (sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x14))
#define SSUSB_IP_PW_STS2				_SSUSB_IP_PW_STS2(mtk_xhci->sif_regs)
/* #define SSUSB_U2_MAC_SYS_RST_B_STS (0x1<<0) */

#define _SSUSB_OTG_STS(sif_base)		(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x18))
#define SSUSB_OTG_STS					_SSUSB_OTG_STS(mtk_xhci->sif_regs)
/* #define SSUSB_IDDIG         (1 << 10) */

#define _SSUSB_U3_CTRL(sif_base, p)		(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x30+(p*0x08)))
#define SSUSB_U3_CTRL(p)				_SSUSB_U3_CTRL(mtk_xhci->sif_regs, p)
/* #define SSUSB_U3_PORT_DIS     (1<<0) */
/* #define SSUSB_U3_PORT_PDN     (1<<1) */
/* #define SSUSB_U3_PORT_HOST_SEL        (1<<2) */
/* #define SSUSB_U3_PORT_CKBG_EN (1<<3) */
/* #define SSUSB_U3_PORT_MAC_RST (1<<4) */
/* #define SSUSB_U3_PORT_PHYD_RST        (1<<5) */

#define _SSUSB_U2_CTRL(sif_base, p)		(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+(0x50)+(p*0x08)))
#define SSUSB_U2_CTRL(p)				_SSUSB_U2_CTRL(mtk_xhci->sif_regs, p)
/* #define SSUSB_U2_PORT_DIS     (1<<0) */
/* #define SSUSB_U2_PORT_PDN     (1<<1) */
/* #define SSUSB_U2_PORT_HOST_SEL        (1<<2) */
/* #define SSUSB_U2_PORT_CKBG_EN (1<<3) */
/* #define SSUSB_U2_PORT_MAC_RST (1<<4) */
/* #define SSUSB_U2_PORT_PHYD_RST        (1<<5) */

#define _SSUSB_IP_CAP(sif_base)			(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x024))
#define SSUSB_IP_CAP					_SSUSB_IP_CAP(mtk_xhci->sif_regs)

#define SSUSB_U3_PORT_NUM(p)			(p & 0xff)
#define SSUSB_U2_PORT_NUM(p)			((p>>8) & 0xff)

#define _SSUSB_SYS_CK_CTRL(sif_base)	(sif_base + (unsigned long)(SIFSLV_IPPC_OFFSET+0x009C))
#define SSUSB_SYS_CK_CTRL				_SSUSB_SYS_CK_CTRL(mtk_xhci->sif_regs)
#define SSUSB_SYS_CK_DIV2_EN			(0x1<<0)

/* ref doc ssusb_xHCI_exclude_port_csr.xlsx */
#define _SSUSB_XHCI_HDMA_CFG(mac_base)	(_SSUSB_XHCI_EXCLUDE_BASE(mac_base) + (unsigned long)0x50)
#define SSUSB_XHCI_HDMA_CFG				_SSUSB_XHCI_HDMA_CFG(mtk_xhci->base_regs)

#define _SSUSB_XHCI_U2PORT_CFG(base)	(_SSUSB_XHCI_EXCLUDE_BASE(base) + (unsigned long)0x78)
#define SSUSB_XHCI_U2PORT_CFG			_SSUSB_XHCI_U2PORT_CFG(mtk_xhci->base_regs)

#define _SSUSB_XHCI_HSCH_CFG2(base)		(_SSUSB_XHCI_EXCLUDE_BASE(base) + (unsigned long)0x7c)
#define SSUSB_XHCI_HSCH_CFG2			 _SSUSB_XHCI_HSCH_CFG2(mtk_xhci->base_regs)
#define XHCI_DRIVER_NAME				"xhci"
#define XHCI_BASE_REGS_ADDR_RES_NAME	"ssusb_base"
#define XHCI_SIF_REGS_ADDR_RES_NAME		"ssusb_sif"
#define XHCI_SIF2_REGS_ADDR_RES_NAME	"ssusb_sif2"

#define K_ALET	(1<<6)
#define K_CRIT	(1<<5)
#define K_ERR	(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

/*Set the debug level for xhci driver*/
extern u32 xhci_debug_level;

extern struct xhci_hcd *mtk_xhci;

#define mtk_xhci_mtk_printk(level, fmt, args...) do { \
		if (xhci_debug_level & level) { \
			pr_debug("[XHCI]" fmt, ## args); \
		} \
	} while (0)

extern int mtk_xhci_ip_init(struct usb_hcd *hcd, struct xhci_hcd *xhci);
extern int get_num_u3_ports(struct xhci_hcd *xhci);
extern int get_num_u2_ports(struct xhci_hcd *xhci);

extern void mtk_xhci_ck_timer_init(struct xhci_hcd *);
extern int mtk_xhci_set(struct usb_hcd *hcd, struct xhci_hcd *xhci);
extern void mtk_xhci_reset(struct xhci_hcd *xhci);
extern bool mtk_is_host_mode(void);

#ifdef CONFIG_USB_MTK_DUALMODE
extern int mtk_xhci_eint_iddig_init(void);
extern void mtk_xhci_switch_init(void);
extern void mtk_xhci_eint_iddig_deinit(void);
extern void mtk_ep_count_inc(void);
extern void mtk_ep_count_dec(void);
extern bool musb_check_ipo_state(void);
#endif

extern int xhci_attrs_init(void);
extern void xhci_attrs_exit(void);

extern void mtk_xhci_wakelock_init(void);
extern void mtk_xhci_wakelock_lock(void);
extern void mtk_xhci_wakelock_unlock(void);

/*
  mediatek probe out
*/
/************************************************************************************/

#define _SW_PRB_OUT_ADDR(sif_base)		((unsigned long)(sif_base + SIFSLV_IPPC_OFFSET + 0xc0))
#define SW_PRB_OUT_ADDR					((unsigned long)_SW_PRB_OUT_ADDR(mtk_xhci->sif_regs))

#define _PRB_MODULE_SEL_ADDR(sif_base)	((unsigned long)(sif_base + SIFSLV_IPPC_OFFSET + 0xbc))
#define PRB_MODULE_SEL_ADDR				((unsigned long)_PRB_MODULE_SEL_ADDR(mtk_xhci->sif_regs))

static inline void mtk_probe_init(const u32 byte)
{
	void __iomem *ptr = (void __iomem *)_PRB_MODULE_SEL_ADDR(mtk_xhci->sif_regs);

	writel(byte, ptr);
}

static inline void mtk_probe_out(const u32 value)
{
	void __iomem *ptr = (void __iomem *)_SW_PRB_OUT_ADDR(mtk_xhci->sif_regs);

	writel(value, ptr);
}

static inline u32 mtk_probe_value(void)
{
	void __iomem *ptr = (void __iomem *) _SW_PRB_OUT_ADDR(mtk_xhci->sif_regs);

	return readl(ptr);
}


#endif
