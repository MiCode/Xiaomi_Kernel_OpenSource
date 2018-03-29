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

#ifndef _XHCI_MTK_POWER_H
#define _XHCI_MTK_POWER_H

#include <linux/usb.h>

void enableXhciAllPortPower(struct xhci_hcd *xhci);
void disableXhciAllPortPower(struct xhci_hcd *xhci);
void enableAllClockPower(struct xhci_hcd *xhci, bool is_reset);
void disableAllClockPower(struct xhci_hcd *xhci);
#if 0
void disablePortClockPower(int port_index, int port_rev);
void enablePortClockPower(int port_index, int port_rev);
#endif

#ifdef CONFIG_USB_MTK_DUALMODE
void mtk_switch2host(void);
void mtk_switch2device(bool skip);
#endif


#ifdef CONFIG_MTK_BQ25896_SUPPORT
extern void bq25890_set_boost_ilim(unsigned int val);
extern void bq25890_otg_en(unsigned int val);
#endif

#ifdef CONFIG_MTK_OTG_PMIC_BOOST_5V
extern unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT);
#endif


extern int set_chr_boost_current_limit(unsigned int current_limit);
extern int set_chr_enable_otg(unsigned int enable);



#if defined(CONFIG_MTK_BQ25896_SUPPORT) \
	|| defined(CONFIG_MTK_OTG_PMIC_BOOST_5V)
#define MTK_OTG_BOOST_5V_SUPPORT
#endif


#endif
