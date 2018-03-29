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

#include "xhci.h"
#include "xhci-mtk-power.h"
#include "xhci-mtk-driver.h"
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/types.h>




#define mtk_power_log(fmt, args...) \
	pr_debug("%s(%d): " fmt, __func__, __LINE__, ##args)


static bool wait_for_value(unsigned long addr, int msk, int value, int ms_intvl, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if ((readl((void __iomem *)addr) & msk) == value)
			return true;
		mdelay(ms_intvl);
	}

	return false;
}

static void mtk_chk_usb_ip_ck_sts(struct xhci_hcd *xhci)
{
	int ret;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	ret =
	    wait_for_value(_SSUSB_IP_PW_STS1(xhci->sif_regs), SSUSB_SYS125_RST_B_STS,
			   SSUSB_SYS125_RST_B_STS, 1, 10);
	if (ret == false)
		mtk_xhci_mtk_printk(K_DEBUG, "sys125_ck is still active!!!\n");

	/* do not check when SSUSB_U2_PORT_PDN = 1, because U2 port stays in reset state */
	if (num_u2_port
	    && !(readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, 0)) & SSUSB_U2_PORT_PDN)) {
		ret =
		    wait_for_value(_SSUSB_IP_PW_STS2(xhci->sif_regs), SSUSB_U2_MAC_SYS_RST_B_STS,
				   SSUSB_U2_MAC_SYS_RST_B_STS, 1, 10);
		if (ret == false)
			mtk_xhci_mtk_printk(K_DEBUG, "mac2_sys_ck is still active!!!\n");
	}

	/* do not check when SSUSB_U3_PORT_PDN = 1, because U3 port stays in reset state */
	if (num_u3_port
	    && !(readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, 0)) & SSUSB_U3_PORT_PDN)) {
		ret =
		    wait_for_value(_SSUSB_IP_PW_STS1(xhci->sif_regs), SSUSB_U3_MAC_RST_B_STS,
				   SSUSB_U3_MAC_RST_B_STS, 1, 10);
		if (ret == false)
			mtk_xhci_mtk_printk(K_DEBUG, "mac3_mac_ck is still active!!!\n");
	}
}

/* set 1 to PORT_POWER of PORT_STATUS register of each port */
void enableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	u32 __iomem *addr;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	pr_debug("%s(%d): port number, u3-%d, u2-%d\n",
		__func__, __LINE__, num_u3_port, num_u2_port);

	for (i = 1; i <= num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		writel(temp, addr);
		while (!(readl(addr) & PORT_POWER))
			;
	}

	for (i = 1; i <= num_u2_port; i++) {
		port_id = i + num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		writel(temp, addr);
		while (!(readl(addr) & PORT_POWER))
			;
	}
}

/* set 0 to PORT_POWER of PORT_STATUS register of each port */
void disableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	void __iomem *addr;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	mtk_xhci_mtk_printk(K_DEBUG, "port number, u3-%d, u2-%d\n", num_u3_port, num_u2_port);

	for (i = 1; i <= num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		 writel(temp, addr);
		while (readl(addr) & PORT_POWER)
			;
	}

	for (i = 1; i <= num_u2_port; i++) {
		port_id = i + num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		writel(temp, addr);
		while (readl(addr) & PORT_POWER)
			;
	}

	xhci_print_registers(mtk_xhci);
}

void enableAllClockPower(struct xhci_hcd *xhci, bool is_reset)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP((xhci->sif_regs))));

	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): u3 port number = %d\n", __func__, __LINE__, num_u3_port);
	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): u2 port number = %d\n", __func__, __LINE__, num_u2_port);

	/* reset whole ip */
	if (is_reset) {
		writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs)) | (SSUSB_IP_SW_RST),
		       (void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs));
		writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs)) &
		       (~SSUSB_IP_SW_RST), (void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs));
	}

	/* disable ip host power down bit --> power on host ip */
	writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL_1(xhci->sif_regs)) & (~SSUSB_IP_PDN),
	       (void __iomem *)_SSUSB_IP_PW_CTRL_1(xhci->sif_regs));

	/* disable all u3 port power down and disable bits --> power on and enable all u3 ports */
	for (i = 0; i < num_u3_port; i++) {
		temp = readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
		temp =
		    (temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS)) | (SSUSB_U3_PORT_HOST_SEL);
		writel(temp, (void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
	}

	/*
	 * FIXME: clock is the correct 30MHz, if the U3 device is enabled
	 */
#if 0
	temp = readl(SSUSB_U3_CTRL(0));
	temp = temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS) & (~SSUSB_U3_PORT_HOST_SEL);
	writel(temp, SSUSB_U3_CTRL(0));
#endif

	/* disable all u2 port power down and disable bits --> power on and enable all u2 ports */
	for (i = 0; i < num_u2_port; i++) {
		temp = readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
		temp =
		    (temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS)) | (SSUSB_U2_PORT_HOST_SEL);
		writel(temp, (void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
	}
	/* msleep(100); */
	mtk_chk_usb_ip_ck_sts(xhci);
}


int get_num_u3_ports(struct xhci_hcd *xhci)
{
	return SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
}

int get_num_u2_ports(struct xhci_hcd *xhci)
{
	return SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP((xhci->sif_regs))));
}

#if 0
/* called after HC initiated */
void disableAllClockPower(void)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));

	/* disable target ports */
	for (i = 0; i < num_u3_port; i++) {
		temp = readl((void __iomem *)SSUSB_U3_CTRL(i));
		temp = temp | SSUSB_U3_PORT_PDN & (~SSUSB_U3_PORT_HOST_SEL);
		writel(temp, (void __iomem *)SSUSB_U3_CTRL(i));
	}

	for (i = 0; i < num_u2_port; i++) {
		temp = readl((void __iomem *)SSUSB_U2_CTRL(i));
		temp = temp | SSUSB_U2_PORT_PDN & (~SSUSB_U2_PORT_HOST_SEL);
		writel(temp, (void __iomem *)SSUSB_U2_CTRL(i));
	}

	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) | (SSUSB_IP_PDN),
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);
	/* msleep(100); */
	mtk_chk_usb_ip_ck_sts();
}
#endif

#if 0
/* (X)disable clock/power of a port */
/* (X)if all ports are disabled, disable IP ctrl power */
/* disable all ports and IP clock/power, this is just mention HW that the power/clock of port */
/* and IP could be disable if suspended. */
/* If doesn't not disable all ports at first, the IP clock/power will never be disabled */
/* (some U2 and U3 ports are binded to the same connection, that is, they will never enter suspend at the same time */
/* port_index: port number */
/* port_rev: 0x2 - USB2.0, 0x3 - USB3.0 (SuperSpeed) */
void disablePortClockPower(int port_index, int port_rev)
{
	u32 temp;
	int real_index;

	real_index = port_index;

	if (port_rev == 0x3) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
		temp = temp | (SSUSB_U3_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
		temp = temp | (SSUSB_U2_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
	}

	writel(readl((void __iomem *)(unsigned long)SSUSB_IP_PW_CTRL_1) | (SSUSB_IP_PDN),
	       (void __iomem *)(unsigned long)SSUSB_IP_PW_CTRL_1);
}

/* if IP ctrl power is disabled, enable it */
/* enable clock/power of a port */
/* port_index: port number */
/* port_rev: 0x2 - USB2.0, 0x3 - USB3.0 (SuperSpeed) */
void enablePortClockPower(int port_index, int port_rev)
{
	u32 temp;
	int real_index;

	real_index = port_index;

	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN),
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);

	if (port_rev == 0x3) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
		temp = temp & (~SSUSB_U3_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
		temp = temp & (~SSUSB_U2_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
	}
}
#endif
