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
#include <mu3phy/mtk-phy.h>
#include "ssusb_io.h"
#include "musb_core.h"

#define IDDIG_EINT_PIN 16

#ifdef CONFIG_SSUSB_MTK_XHCI
void mtk_xhci_ip_init(struct ssusb_mtk *ssusb);
void mtk_xhci_ip_exit(struct ssusb_mtk *ssusb);
void ssusb_mode_switch_manual(struct ssusb_mtk *ssusb, int to_host);

#endif

bool mtk_is_host_mode(void);
int mtk_otg_switch_init(struct ssusb_mtk *ssusb);
void mtk_otg_switch_exit(struct ssusb_mtk *ssusb);
int ssusb_host_init(struct ssusb_mtk *ssusb);
void ssusb_host_exit(struct ssusb_mtk *ssusb);
int ssusb_host_enable(struct ssusb_mtk *ssusb);
int ssusb_host_disable(struct ssusb_mtk *ssusb);


/* from charge driver */
extern void tbl_charger_otg_vbus(int mode);

/*
  mediatek probe out
*/
/************************************************************************************/
#if 0
#define SW_PRB_OUT_ADDR	(SIFSLV_IPPC+0xc0)	/* 0xf00447c0 */
#define PRB_MODULE_SEL_ADDR	(SIFSLV_IPPC+0xbc)	/* 0xf00447bc */

static inline void mtk_probe_init(const u32 byte)
{
	__u32 __iomem *ptr = (__u32 __iomem *) PRB_MODULE_SEL_ADDR;

	writel(byte, ptr);
}

static inline void mtk_probe_out(const u32 value)
{
	__u32 __iomem *ptr = (__u32 __iomem *) SW_PRB_OUT_ADDR;

	writel(value, ptr);
}

static inline u32 mtk_probe_value(void)
{
	__u32 __iomem *ptr = (__u32 __iomem *) SW_PRB_OUT_ADDR;

	return readl(ptr);
}
#endif

#endif
