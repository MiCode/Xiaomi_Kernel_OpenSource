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

#include "mtk-phy.h"

#ifdef CONFIG_PROJECT_PHY
/*#include <mach/mt_pm_ldo.h>*/
#include <linux/clk.h>
#include <linux/io.h>
#include "mtk-phy-asic.h"
#include "mu3d_hal_osal.h"
#ifdef CONFIG_MTK_UART_USB_SWITCH
#include "mu3d_hal_usb_drv.h"
#endif

#include <mt-plat/upmu_common.h>
#include "mtk_spm_resource_req.h"
#include "mtk_idle.h"
/*TODO#include "mtk_clk_id.h" */
#include "musb_core.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#include <mt-plat/mtk_chip.h>
#include "mtk_devinfo.h"

static int usb20_phy_rev6;
static struct clk *ssusb_clk;
static struct clk *sys_ck;
static DEFINE_SPINLOCK(mu3phy_clock_lock);
bool sib_mode;
static struct regulator *reg_vusb;
static struct regulator *reg_va09;

enum VA09_OP {
	VA09_OP_OFF = 0,
	VA09_OP_ON,
};

static void VA09_operation(int op, bool force)
{
	int ret = 0;
	static bool on;

	/* only musb_speed or force could pass it */
	if (!musb_speed && !force)
		return;

	if (!reg_va09)
		return;

	if (op == VA09_OP_ON && !on) {
		ret = regulator_enable(reg_va09);
		if (ret < 0) {
			pr_notice("regulator_enable va09 failed: %d\n", ret);
			return;
		}
		on = true;

	} else if (op == VA09_OP_OFF && on) {
		ret = regulator_disable(reg_va09);
		if (ret < 0) {
			pr_notice("regulator_enable va09 failed: %d\n", ret);
			return;
		}
		on = false;
	}
}

static int dpidle_status = USB_DPIDLE_ALLOWED;
static DEFINE_SPINLOCK(usb_hal_dpidle_lock);
#define DPIDLE_TIMER_INTERVAL_MS 30
static void issue_dpidle_timer(void);
static void dpidle_timer_wakeup_func(unsigned long data)
{
	struct timer_list *timer = (struct timer_list *)data;

	{
		static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 1);
		static int skip_cnt;

		if (__ratelimit(&ratelimit)) {
			os_printk(K_INFO, "dpidle_timer<%p> alive, skip_cnt<%d>\n",
				timer, skip_cnt);
			skip_cnt = 0;
		} else
			skip_cnt++;
	}
	os_printk(K_DEBUG, "dpidle_timer<%p> alive...\n", timer);
	if (dpidle_status == USB_DPIDLE_TIMER)
		issue_dpidle_timer();
	kfree(timer);
}
static void issue_dpidle_timer(void)
{
	struct timer_list *timer;

	timer = kzalloc(sizeof(struct timer_list), GFP_ATOMIC);
	if (!timer)
		return;

	os_printk(K_DEBUG, "add dpidle_timer<%p>\n", timer);
	init_timer(timer);
	timer->function = dpidle_timer_wakeup_func;
	timer->data = (unsigned long)timer;
	timer->expires = jiffies + msecs_to_jiffies(DPIDLE_TIMER_INTERVAL_MS);
	add_timer(timer);
}

void usb_hal_dpidle_request(int mode)
{
	unsigned long flags;

#ifdef U3_COMPLIANCE
	os_printk(K_INFO, "%s, U3_COMPLIANCE, fake to USB_DPIDLE_FORBIDDEN\n",
		__func__);
	mode = USB_DPIDLE_FORBIDDEN;
#endif

	spin_lock_irqsave(&usb_hal_dpidle_lock, flags);

	/* update dpidle_status */
	dpidle_status = mode;

	switch (mode) {
	case USB_DPIDLE_ALLOWED:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, SPM_RESOURCE_RELEASE);
		os_printk(K_INFO, "USB_DPIDLE_ALLOWED\n");
		break;
	case USB_DPIDLE_FORBIDDEN:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, SPM_RESOURCE_ALL);
		{
			static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

			if (__ratelimit(&ratelimit))
				os_printk(K_INFO, "USB_DPIDLE_FORBIDDEN\n");
		}
		break;
	case USB_DPIDLE_SRAM:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB,
				SPM_RESOURCE_CK_26M | SPM_RESOURCE_MAINPLL);
		{
			static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);
			static int skip_cnt;

			if (__ratelimit(&ratelimit)) {
				os_printk(K_INFO, "USB_DPIDLE_SRAM, skip_cnt<%d>\n",
					skip_cnt);
				skip_cnt = 0;
			} else
				skip_cnt++;
		}
		break;
	case USB_DPIDLE_TIMER:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB,
				SPM_RESOURCE_CK_26M | SPM_RESOURCE_MAINPLL);
		os_printk(K_INFO, "USB_DPIDLE_TIMER\n");
		issue_dpidle_timer();
		break;
	default:
		os_printk(K_WARNIN, "[ERROR] Are you kidding!?!?\n");
		break;
	}

	spin_unlock_irqrestore(&usb_hal_dpidle_lock, flags);
}

static bool usb_enable_clock(bool enable)
{
	static int count;
	unsigned long flags;

	if (!ssusb_clk || IS_ERR(ssusb_clk)) {
		pr_notice("clock not ready, ssusb_clk:%p", ssusb_clk);
		return -1;
	}

	if (!sys_ck || IS_ERR(sys_ck)) {
		pr_notice("clock not ready, sys_ck:%p", sys_ck);
		return -1;
	}

	spin_lock_irqsave(&mu3phy_clock_lock, flags);
	os_printk(K_INFO, "CG, enable<%d>, count<%d>\n", enable, count);

	if (enable && count == 0) {
		usb_hal_dpidle_request(USB_DPIDLE_FORBIDDEN);
		if (clk_enable(ssusb_clk) != 0)
			pr_notice("ssusb_ref_clk enable fail\n");
		if (clk_enable(sys_ck) != 0)
			pr_notice("sys_ck enable fail\n");
	} else if (!enable && count == 1) {
		clk_disable(ssusb_clk);
		clk_disable(sys_ck);
		usb_hal_dpidle_request(USB_DPIDLE_ALLOWED);
	}

	if (enable)
		count++;
	else
		count = (count == 0) ? 0 : (count - 1);

	spin_unlock_irqrestore(&mu3phy_clock_lock, flags);

	return 0;
}

void usb20_pll_settings(bool host, bool forceOn)
{
}

void usb20_rev6_setting(int value, bool is_update)
{
	if (is_update)
		usb20_phy_rev6 = value;

	U3PhyWriteField32(
		(phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_PHY_REV_6_OFST, RG_USB20_PHY_REV_6, value);
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
bool in_uart_mode;
void uart_usb_switch_dump_register(void)
{
	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);
	udelay(250);

# ifdef CONFIG_MTK_FPGA
	pr_debug("[MUSB]addr: 0x6B, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYDTM0 + 0x3));
	pr_debug("[MUSB]addr: 0x6E, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYDTM1 + 0x2));
	pr_debug("[MUSB]addr: 0x22, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYACR4 + 0x2));
	pr_debug("[MUSB]addr: 0x68, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYDTM0));
	pr_debug("[MUSB]addr: 0x6A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYDTM0 + 0x2));
	pr_debug("[MUSB]addr: 0x1A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t)
			(uintptr_t)U3D_U2PHYACR6 + 0x2));
#else
#if 0
	os_printk(K_INFO, "[MUSB]addr: 0x6B, value: %x\n",
		U3PhyReadReg8(U3D_U2PHYDTM0 + 0x3));
	os_printk(K_INFO, "[MUSB]addr: 0x6E, value: %x\n",
		U3PhyReadReg8(U3D_U2PHYDTM1 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x22, value: %x\n",
		U3PhyReadReg8(U3D_U2PHYACR4 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: %x\n",
		U3PhyReadReg8(U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6A, value: %x\n",
		U3PhyReadReg8(U3D_U2PHYDTM0 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x1A, value: %x\n",
		U3PhyReadReg8(U3D_USBPHYACR6 + 0x2));
#else
	os_printk(K_INFO, "[MUSB]addr: 0x18, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6));
	os_printk(K_INFO, "[MUSB]addr: 0x20, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6C, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1));
#endif
#endif

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);

	/*os_printk(K_INFO, "[MUSB]addr: 0x110020B0 (UART0), value: %x\n\n", */
	/*	  DRV_Reg8(ap_uart0_base + 0xB0)); */
}

bool usb_phy_check_in_uart_mode(void)
{
	PHY_INT32 usb_port_mode;

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);

	udelay(250);
	usb_port_mode = U3PhyReadReg32(
		(phys_addr_t) (uintptr_t) U3D_U2PHYDTM0) >> RG_UART_MODE_OFST;

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);
	os_printk(K_INFO, "%s+ usb_port_mode = %d\n", __func__, usb_port_mode);

	if (usb_port_mode == 0x1)
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	if (usb_phy_check_in_uart_mode()) {
		os_printk(K_INFO, "%s+ UART_MODE\n", __func__);
		return;
	}

	os_printk(K_INFO, "%s+ USB_MODE\n", __func__);

	VA09_operation(VA09_OP_ON, false);

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);
	udelay(250);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 0);

	/* RG_SUSPENDM to 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	/* force suspendm = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);

	/* rg_uart_mode = 2'b01 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_UART_MODE_OFST, RG_UART_MODE, 1);

	/* force_uart_i = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_I_OFST, FORCE_UART_I, 0);

	/* force_uart_bias_en = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_BIAS_EN_OFST, FORCE_UART_BIAS_EN, 1);

	/* force_uart_tx_oe = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_TX_OE_OFST, FORCE_UART_TX_OE, 1);

	/* force_uart_en = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_EN_OFST, FORCE_UART_EN, 1);

	/* RG_UART_BIAS_EN = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_BIAS_EN_OFST, RG_UART_BIAS_EN, 1);

	/* RG_UART_TX_OE = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_TX_OE_OFST, RG_UART_TX_OE, 1);

	/* RG_UART_EN = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_EN_OFST, RG_UART_EN, 1);

	/* RG_USB20_DM_100K_EN = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_DM_100K_EN_OFST, RG_USB20_DM_100K_EN, 1);

	/* force_linestate = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		FORCE_LINESTATE_OFST, FORCE_LINESTATE, 1);

	/* RG_LINESTATE = 2'b01 (J State) */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_LINESTATE_OFST, RG_LINESTATE, 0x1);

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);

	/* SET USB to UART GPIO to UART0 */
	DRV_WriteReg32(ap_uart0_base + 0x6E0,
		(DRV_Reg32(ap_uart0_base + 0x6E0) | (1<<20)));

	in_uart_mode = true;
}


void usb_phy_switch_to_usb(void)
{
	in_uart_mode = false;

	/* force_uart_i = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_I_OFST, FORCE_UART_I, 1);

	/* force_uart_en = 1'b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_EN_OFST, FORCE_UART_EN, 1);

	/* RG_UART_EN = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_EN_OFST, RG_UART_EN, 0);

	/* RG_USB20_DM_100K_EN = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_DM_100K_EN_OFST, RG_USB20_DM_100K_EN, 0);

	/* rg_uart_mode = 2'b00 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_UART_MODE_OFST, RG_UART_MODE, 0);

	/* force_dp_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DP_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN, 0);

	/* force_dm_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DM_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN, 0);

	/* force_xcversel        1'b0 */
	/* U3D_U2PHYDTM0 FORCE_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 0);

	/* force_termsel 1'b0 */
	/* U3D_U2PHYDTM0 FORCE_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_TERMSEL_OFST, FORCE_TERMSEL, 0);

	/* force_datain  1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DATAIN_OFST, FORCE_DATAIN, 0);

	/* force_linestate = 1'b0 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		FORCE_LINESTATE_OFST, FORCE_LINESTATE, 0);

	/* CLEAR USB to UART GPIO to UART0 */
	DRV_WriteReg32(ap_uart0_base + 0x6E0,
		(DRV_Reg32(ap_uart0_base + 0x6E0) & ~(1<<20)));

	phy_init_soc(u3phy);

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(false);
}
#endif

#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT

#define USB30_PHYD_PIPE0 (SSUSB_SIFSLV_U3PHYD_BASE+0x40)
#define USB30_PHYD_RX0 (SSUSB_SIFSLV_U3PHYD_BASE+0x2c)
#define USB30_PHYD_MIX0 (SSUSB_SIFSLV_U3PHYD_BASE+0x0)
#define USB30_PHYD_T2RLB (SSUSB_SIFSLV_U3PHYD_BASE+0x30)

bool u3_loop_back_test(void)
{
	int reg;
	bool loop_back_ret = false;

	VA09_operation(VA09_OP_ON, true);
	usb_enable_clock(true);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
		RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 1);

	/*SSUSB_IP_SW_RST = 0*/
	writel(readl(U3D_SSUSB_IP_PW_CTRL0)&~(SSUSB_IP_SW_RST),
		U3D_SSUSB_IP_PW_CTRL0);
	/*SSUSB_IP_HOST_PDN = 0*/
	writel(readl(U3D_SSUSB_IP_PW_CTRL1)&~(SSUSB_IP_HOST_PDN),
		U3D_SSUSB_IP_PW_CTRL1);
	/*SSUSB_IP_DEV_PDN = 0*/
	writel(readl(U3D_SSUSB_IP_PW_CTRL2)&~(SSUSB_IP_DEV_PDN),
		U3D_SSUSB_IP_PW_CTRL2);
	/*SSUSB_IP_PCIE_PDN = 0*/
	writel(readl(U3D_SSUSB_IP_PW_CTRL3)&~(SSUSB_IP_PCIE_PDN),
		U3D_SSUSB_IP_PW_CTRL3);
	/*SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	writel(readl(
		U3D_SSUSB_U3_CTRL_0P)&~(SSUSB_IP_PCIE_PDN | SSUSB_U3_PORT_PDN),
		U3D_SSUSB_U3_CTRL_0P);

	mdelay(10);

	writel((readl(USB30_PHYD_PIPE0)&~(0x01<<30))|0x01<<30,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x01<<28))|0x00<<28,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x03<<26))|0x01<<26,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x03<<24))|0x00<<24,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x01<<22))|0x00<<22,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x01<<21))|0x00<<21,
							USB30_PHYD_PIPE0);
	writel((readl(USB30_PHYD_PIPE0)&~(0x01<<20))|0x01<<20,
							USB30_PHYD_PIPE0);
	mdelay(10);

	/*T2R loop back disable*/
	writel((readl(USB30_PHYD_RX0)&~(0x01<<15))|0x00<<15,
							USB30_PHYD_RX0);
	mdelay(10);

	/* TSEQ lock detect threshold */
	writel((readl(USB30_PHYD_MIX0)&~(0x07<<24))|0x07<<24,
							USB30_PHYD_MIX0);
	/* set default TSEQ polarity check value = 1 */
	writel((readl(USB30_PHYD_MIX0)&~(0x01<<28))|0x01<<28,
							USB30_PHYD_MIX0);
	/* TSEQ polarity check enable */
	writel((readl(USB30_PHYD_MIX0)&~(0x01<<29))|0x01<<29,
							USB30_PHYD_MIX0);
	/* TSEQ decoder enable */
	writel((readl(USB30_PHYD_MIX0)&~(0x01<<30))|0x01<<30,
							USB30_PHYD_MIX0);
	mdelay(10);

	/* set T2R loop back TSEQ length (x 16us) */
	writel((readl(USB30_PHYD_T2RLB)&~(0xff<<0))|0xF0<<0,
							USB30_PHYD_T2RLB);
	/* set T2R loop back BDAT reset period (x 16us) */
	writel((readl(USB30_PHYD_T2RLB)&~(0x0f<<12))|0x0F<<12,
							USB30_PHYD_T2RLB);
	/* T2R loop back pattern select */
	writel((readl(USB30_PHYD_T2RLB)&~(0x03<<8))|0x00<<8,
							USB30_PHYD_T2RLB);
	mdelay(10);

	/* T2R loop back serial mode */
	writel((readl(USB30_PHYD_RX0)&~(0x01<<13))|0x01<<13,
							USB30_PHYD_RX0);
	/* T2R loop back parallel mode = 0 */
	writel((readl(USB30_PHYD_RX0)&~(0x01<<12))|0x00<<12,
							USB30_PHYD_RX0);
	/* T2R loop back mode enable */
	writel((readl(USB30_PHYD_RX0)&~(0x01<<11))|0x01<<11,
							USB30_PHYD_RX0);
	/* T2R loop back enable */
	writel((readl(USB30_PHYD_RX0)&~(0x01<<15))|0x01<<15,
							USB30_PHYD_RX0);
	mdelay(100);

	reg = U3PhyReadReg32((phys_addr_t) (uintptr_t)
		(SSUSB_SIFSLV_U3PHYD_BASE+0xb4));
	os_printk(K_INFO, "read back             : 0x%x\n", reg);
	os_printk(K_INFO, "read back t2rlb_lock  : %d\n", (reg>>2)&0x01);
	os_printk(K_INFO, "read back t2rlb_pass  : %d\n", (reg>>3)&0x01);
	os_printk(K_INFO, "read back t2rlb_passth: %d\n", (reg>>4)&0x01);

	if ((reg&0x0E) == 0x0E)
		loop_back_ret = true;
	else
		loop_back_ret = false;

	return loop_back_ret;
}
#endif

#ifdef CONFIG_MTK_SIB_USB_SWITCH
struct wakeup_source sib_wakelock;
void usb_phy_sib_enable_switch(bool enable)
{
	static int inited;

	if (!inited) {
		os_printk(K_INFO, "%s wake_lock_init\n", __func__);
		wakeup_source_init(&sib_wakelock, "SIB.lock");
		inited = 1;
	}

	VA09_operation(VA09_OP_ON, true);
	usb_enable_clock(true);
	udelay(250);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
		RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 1);

	/* SSUSB_IP_SW_RST = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_ippc_base + 0x0),
		0x00031000);
	/* SSUSB_IP_HOST_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_ippc_base + 0x4),
		0x00000000);
	/* SSUSB_IP_DEV_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_ippc_base + 0x8),
		0x00000000);
	/* SSUSB_IP_PCIE_PDN = 0 */
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_ippc_base + 0xC),
		0x00000000);
	/* SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_ippc_base + 0x30),
		0x0000000C);

	/*
	 * USBMAC mode is 0x62910002 (bit 1)
	 * MDSIB  mode is 0x62910008 (bit 3)
	 * 0x0629 just likes a signature. Can't be removed.
	 */
	if (enable) {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t)
			SSUSB_SIFSLV_CHIP_BASE, 0x62910008);
		sib_mode = true;
		if (!sib_wakelock.active)
			__pm_stay_awake(&sib_wakelock);
	} else {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t)
			SSUSB_SIFSLV_CHIP_BASE, 0x62910002);
		sib_mode = false;
		if (sib_wakelock.active)
			__pm_relax(&sib_wakelock);
	}
}

bool usb_phy_sib_enable_switch_status(void)
{
	int reg;
	bool ret;

	VA09_operation(VA09_OP_ON, true);

	usb_enable_clock(true);
	udelay(250);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
		RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 1);

	reg = U3PhyReadReg32((phys_addr_t) (uintptr_t) SSUSB_SIFSLV_CHIP_BASE);
	if (reg == 0x62910008)
		ret = true;
	else
		ret = false;

	return ret;
}
#endif


#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
int usb2jtag_usb_init(void)
{
	struct device_node *node = NULL;
	void __iomem *usb3_sif2_base;
	u32 temp;

	node = of_find_compatible_node(NULL, NULL, "mediatek,usb3");
	if (!node) {
		pr_notice("[USB2JTAG] map node @ mediatek,usb3 failed\n");
		return -1;
	}

	usb3_sif2_base = of_iomap(node, 2);
	if (!usb3_sif2_base) {
		pr_notice("[USB2JTAG] iomap usb3_sif2_base failed\n");
		return -1;
	}

	/* rg_usb20_gpio_ctl: bit[9] = 1 */
	temp = readl(usb3_sif2_base + 0x320);
	writel(temp | (1 << 9), usb3_sif2_base + 0x320);

	/* RG_USB20_BC11_SW_EN: bit[23] = 0 */
	temp = readl(usb3_sif2_base + 0x318);
	writel(temp & ~(1 << 23), usb3_sif2_base + 0x318);

	/* RG_USB20_BGR_EN: bit[0] = 1 */
	temp = readl(usb3_sif2_base + 0x300);
	writel(temp | (1 << 0), usb3_sif2_base + 0x300);

	/* rg_sifslv_mac_bandgap_en: bit[17] = 0 */
	temp = readl(usb3_sif2_base + 0x308);
	writel(temp & ~(1 << 17), usb3_sif2_base + 0x308);

	/* wait stable */
	mdelay(1);

	iounmap(usb3_sif2_base);

	return 0;
}
#endif

#define VAL_MAX_WIDTH_2	0x3
#define VAL_MAX_WIDTH_3	0x7
void usb_phy_tuning(void)
{
	static bool inited;
	static s32 u2_vrt_ref, u2_term_ref, u2_enhance;
	static struct device_node *of_node;

	if (!inited) {
		u2_vrt_ref = u2_term_ref = u2_enhance = -1;
		of_node = of_find_compatible_node(NULL, NULL,
			"mediatek,phy_tuning");
		if (of_node) {
			/* value won't be updated if property not being found */
			of_property_read_u32(of_node, "u2_vrt_ref",
				(u32 *) &u2_vrt_ref);
			of_property_read_u32(of_node, "u2_term_ref",
				(u32 *) &u2_term_ref);
			of_property_read_u32(of_node, "u2_enhance",
				(u32 *) &u2_enhance);
		}
		inited = true;
	} else if (!of_node)
		return;

	if (u2_vrt_ref != -1) {
		if (u2_vrt_ref <= VAL_MAX_WIDTH_3) {
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_USBPHYACR1, RG_USB20_VRT_VREF_SEL_OFST,
				RG_USB20_VRT_VREF_SEL, u2_vrt_ref);
		}
	}
	if (u2_term_ref != -1) {
		if (u2_term_ref <= VAL_MAX_WIDTH_3) {
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_USBPHYACR1, RG_USB20_TERM_VREF_SEL_OFST,
				RG_USB20_TERM_VREF_SEL, u2_term_ref);
		}
	}
	if (u2_enhance != -1) {
		if (u2_enhance <= VAL_MAX_WIDTH_2) {
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_USBPHYACR6, RG_USB20_PHY_REV_6_OFST,
				RG_USB20_PHY_REV_6, u2_enhance);
		}
	}
}

PHY_INT32 phy_init_soc(struct u3phy_info *info)
{
	os_printk(K_INFO, "%s+\n", __func__);

	/*This power on sequence refers to Sheet .1 of  */
	/*"6593_USB_PORT0_PWR Sequence 20130729.xls" */

	VA09_operation(VA09_OP_ON, false);

	/* f_fusb30_ck:125MHz */
	usb_enable_clock(true);

	/*Wait 50 usec */
	udelay(250);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
		RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 1);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode)
		goto reg_done;
#endif

	/*switch to USB function. (system register, force ip into usb mode) */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
	FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_EN_OFST, RG_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
	/*DP/DM BC1.1 path Disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_DP_100K_MODE_OFST, RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		USB20_DP_100K_EN_OFST, USB20_DP_100K_EN, 0);
	/*dm_100k disable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_DM_100K_EN_OFST, RG_USB20_DM_100K_EN, 0);
	/*OTG Enable */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_OTG_VBUSCMP_EN_OFST, RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Release force suspendm.  (force_suspendm=0) */
	/*(let suspendm=1, enable usb 480MHz pll) */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);

	/*Wait 800 usec */
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		FORCE_VBUSVALID_OFST, FORCE_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		FORCE_AVALID_OFST, FORCE_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		FORCE_SESSEND_OFST, FORCE_SESSEND, 1);

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);

#ifdef CONFIG_MTK_UART_USB_SWITCH
reg_done:
#endif
	os_printk(K_DEBUG, "%s-\n", __func__);

	return PHY_TRUE;
}

PHY_INT32 u2_slew_rate_calibration(struct u3phy_info *info)
{
	PHY_INT32 i = 0;
	PHY_INT32 fgRet = 0;
	PHY_INT32 u4FmOut = 0;
	PHY_INT32 u4Tmp = 0;

	os_printk(K_DEBUG, "%s\n", __func__);

	/* => RG_USB20_HSTX_SRCAL_EN = 1 */
	/* enable USB ring oscillator */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5,
		RG_USB20_HSTX_SRCAL_EN_OFST, RG_USB20_HSTX_SRCAL_EN, 1);

	/* wait 1us */
	udelay(1);

	/* => USBPHY base address + 0x110 = 1 */
	/* Enable free run clock */
	U3PhyWriteField32((phys_addr_t) (uintptr_t)
		(SSUSB_SIFSLV_FM_BASE + 0x10), RG_FRCK_EN_OFST,
		RG_FRCK_EN, 0x1);

	/* => USBPHY base address + 0x100 = 0x04 */
	/* Setting cyclecnt */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (SSUSB_SIFSLV_FM_BASE)
			  , RG_CYCLECNT_OFST, RG_CYCLECNT, 0x400);

	/* => USBPHY base address + 0x100 = 0x01 */
	/* Enable frequency meter */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (SSUSB_SIFSLV_FM_BASE)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0x1);

	/* USB_FM_VLD, should be 1'b1, Read frequency valid */
	os_printk(K_DEBUG, "Freq_Valid=(0x%08X)\n",
		  U3PhyReadReg32((phys_addr_t) (uintptr_t)
			(SSUSB_SIFSLV_FM_BASE + 0x10)));

	mdelay(1);

	/* wait for FM detection done, set 10ms timeout */
	for (i = 0; i < 10; i++) {
		/* => USBPHY base address + 0x10C = FM_OUT */
		/* Read result */
		u4FmOut = U3PhyReadReg32((phys_addr_t) (uintptr_t)
			(SSUSB_SIFSLV_FM_BASE + 0xC));
		os_printk(K_DEBUG, "FM_OUT value: u4FmOut = %d(0x%08X)\n",
			u4FmOut, u4FmOut);

		/* check if FM detection done */
		if (u4FmOut != 0) {
			fgRet = 0;
			os_printk(K_DEBUG, "FM detection done! loop = %d\n", i);
			break;
		}
		fgRet = 1;
		mdelay(1);
	}
	/* => USBPHY base address + 0x100 = 0x00 */
	/* Disable Frequency meter */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (SSUSB_SIFSLV_FM_BASE)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0);

	/* => USBPHY base address + 0x110 = 0x00 */
	/* Disable free run clock */
	U3PhyWriteField32((phys_addr_t) (uintptr_t)
		(SSUSB_SIFSLV_FM_BASE + 0x10), RG_FRCK_EN_OFST, RG_FRCK_EN, 0);

	/* RG_USB20_HSTX_SRCTRL[2:0] = (1024/FM_OUT) */
	/* * reference clock frequency * 0.028 */
	if (u4FmOut == 0) {
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5,
			RG_USB20_HSTX_SRCTRL_OFST, RG_USB20_HSTX_SRCTRL, 0x4);
		fgRet = 1;
	} else {
		/* set reg = (1024/FM_OUT) * REF_CK * U2_SR_COEF_E60802 */
		/*  / 1000 (round to the nearest digits) */
		/* u4Tmp = (((1024 * REF_CK * U2_SR_COEF_E60802) / u4FmOut) */
		/* + 500) / 1000; */
		u4Tmp = (1024 * REF_CK * U2_SR_COEF_E60802) / (u4FmOut * 1000);
		os_printk(K_DEBUG, "SR calibration value u1SrCalVal = %d\n",
			(PHY_UINT8) u4Tmp);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5,
			RG_USB20_HSTX_SRCTRL_OFST, RG_USB20_HSTX_SRCTRL, u4Tmp);
	}

	/* => RG_USB20_HSTX_SRCAL_EN = 0 */
	/* disable USB ring oscillator */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5,
		RG_USB20_HSTX_SRCAL_EN_OFST, RG_USB20_HSTX_SRCAL_EN, 0);

	return fgRet;
}

void usb_phy_savecurrent(unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (sib_mode) {
		pr_notice("%s sib_mode can't savecurrent\n", __func__);
		return;
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode)
		goto reg_done;
#endif

	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 FORCE_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 RG_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_EN_OFST, RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL, 0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN 1'b0 */
	/* U3D_USBPHYACR6 RG_USB20_BC11_SW_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 0);

	/*OTG Disable */
	/* RG_USB20_OTG_VBUSCMP_EN 1b0 */
	/* U3D_USBPHYACR6 RG_USB20_OTG_VBUSCMP_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_OTG_VBUSCMP_EN_OFST, RG_USB20_OTG_VBUSCMP_EN, 0);

	/*let suspendm=1, enable usb 480MHz pll */
	/* RG_SUSPENDM 1'b1 */
	/* U3D_U2PHYDTM0 RG_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	/*force_suspendm=1 */
	/* force_suspendm        1'b1 */
	/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);

	/*Wait USBPLL stable. */
	/* Wait 2 ms. */
	udelay(2000);

	/* RG_DPPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 RG_DPPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 1);

	/* RG_DMPULLDOWN 1'b1 */
	/* U3D_U2PHYDTM0 RG_DMPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 1);

	/* RG_XCVRSEL[1:0] 2'b01 */
	/* U3D_U2PHYDTM0 RG_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_XCVRSEL_OFST, RG_XCVRSEL, 0x1);

	/* RG_TERMSEL    1'b1 */
	/* U3D_U2PHYDTM0 RG_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_TERMSEL_OFST, RG_TERMSEL, 1);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 RG_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DATAIN_OFST, RG_DATAIN, 0);

	/* force_dp_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DP_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN, 1);

	/* force_dm_pulldown     1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DM_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN, 1);

	/* force_xcversel        1'b1 */
	/* U3D_U2PHYDTM0 FORCE_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 1);

	/* force_termsel 1'b1 */
	/* U3D_U2PHYDTM0 FORCE_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_TERMSEL_OFST, FORCE_TERMSEL, 1);

	/* force_datain  1'b1 */
	/* U3D_U2PHYDTM0 FORCE_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DATAIN_OFST, FORCE_DATAIN, 1);

	/* wait 800us */
	udelay(800);

	/*let suspendm=0, set utmi into analog power down */
	/* RG_SUSPENDM 1'b0 */
	/* U3D_U2PHYDTM0 RG_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_SUSPENDM_OFST, RG_SUSPENDM, 0);

	/* wait 1us */
	udelay(1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_VBUSVALID_OFST, RG_VBUSVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_AVALID_OFST, RG_AVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_SESSEND_OFST, RG_SESSEND, 1);

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);

#ifdef CONFIG_MTK_UART_USB_SWITCH
reg_done:
#endif
	/* TODO:
	 * Turn off internal 48Mhz PLL if there is no other hardware module is
	 * using the 48Mhz clock -the control register is in clock document
	 * Turn off SSUSB reference clock (26MHz)
	 */
	if (clk_on) {
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
			RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 0);

		/* Wait 10 usec. */
		udelay(10);

		/* f_fusb30_ck:125MHz */
		usb_enable_clock(false);

		VA09_operation(VA09_OP_OFF, false);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

void usb_phy_recover(unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (!clk_on) {
		VA09_operation(VA09_OP_ON, false);

		/* f_fusb30_ck:125MHz */
		usb_enable_clock(true);

		/* Wait 50 usec. (PHY 3.3v & 1.8v power stable time) */
		udelay(250);

		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG1,
			RG_SSUSB_VUSB09_ON_OFST, RG_SSUSB_VUSB09_ON, 1);
	}
#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode)
		goto reg_done;
#endif

	/*switch to USB function. (system register, force ip into usb mode) */
	/* force_uart_en      1'b0 */
	/* U3D_U2PHYDTM0 FORCE_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	/* RG_UART_EN         1'b0 */
	/* U3D_U2PHYDTM1 RG_UART_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_UART_EN_OFST, RG_UART_EN, 0);
	/* rg_usb20_gpio_ctl  1'b0 */
	/* U3D_U2PHYACR4 RG_USB20_GPIO_CTL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL, 0);
	/* usb20_gpio_mode       1'b0 */
	/* U3D_U2PHYACR4 USB20_GPIO_MODE */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4,
		USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	/*Release force suspendm. (force_suspendm=0) */
	/*(let suspendm=1, enable usb 480MHz pll) */
	/*force_suspendm        1'b0 */
	/* U3D_U2PHYDTM0 FORCE_SUSPENDM */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);

	/* RG_DPPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 RG_DPPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 0);

	/* RG_DMPULLDOWN 1'b0 */
	/* U3D_U2PHYDTM0 RG_DMPULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 0);

	/* RG_XCVRSEL[1:0]       2'b00 */
	/* U3D_U2PHYDTM0 RG_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_XCVRSEL_OFST, RG_XCVRSEL, 0);

	/* RG_TERMSEL    1'b0 */
	/* U3D_U2PHYDTM0 RG_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_TERMSEL_OFST, RG_TERMSEL, 0);

	/* RG_DATAIN[3:0]        4'b0000 */
	/* U3D_U2PHYDTM0 RG_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		RG_DATAIN_OFST, RG_DATAIN, 0);

	/* force_dp_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DP_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN, 0);

	/* force_dm_pulldown     1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DM_PULLDOWN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN, 0);

	/* force_xcversel        1'b0 */
	/* U3D_U2PHYDTM0 FORCE_XCVRSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 0);

	/* force_termsel 1'b0 */
	/* U3D_U2PHYDTM0 FORCE_TERMSEL */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_TERMSEL_OFST, FORCE_TERMSEL, 0);

	/* force_datain  1'b0 */
	/* U3D_U2PHYDTM0 FORCE_DATAIN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0,
		FORCE_DATAIN_OFST, FORCE_DATAIN, 0);

	/*DP/DM BC1.1 path Disable */
	/* RG_USB20_BC11_SW_EN   1'b0 */
	/* U3D_USBPHYACR6 RG_USB20_BC11_SW_EN */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 0);

	/*OTG Enable */
	/* RG_USB20_OTG_VBUSCMP_EN       1b1 */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_OTG_VBUSCMP_EN_OFST, RG_USB20_OTG_VBUSCMP_EN, 1);

	/* Wait 800 usec */
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_VBUSVALID_OFST, RG_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_AVALID_OFST, RG_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1,
		RG_SESSEND_OFST, RG_SESSEND, 0);

	/* EFUSE related sequence */
	{
		u32 evalue;

		/* [4:0] => RG_USB20_INTR_CAL[4:0] */
		evalue = (get_devinfo_with_index(108) & (0x1f<<0)) >> 0;
		if (evalue) {
			os_printk(K_INFO, "apply efuse setting, RG_USB20_INTR_CAL=0x%x\n",
				evalue);
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_USBPHYACR1, RG_USB20_INTR_CAL_OFST,
				RG_USB20_INTR_CAL, evalue);
		} else
			os_printk(K_DEBUG, "!evalue\n");

		/* [21:16] (BGR_code) => RG_SSUSB_IEXT_INTR_CTRL[5:0] */
		evalue = (get_devinfo_with_index(107) & (0x3f << 16)) >> 16;
		if (evalue) {
			os_printk(K_INFO, "apply efuse setting, RG_SSUSB_IEXT_INTR_CTRL=0x%x\n",
				evalue);
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_USB30_PHYA_REG0, 10, (0x3f<<10), evalue);
		} else
			os_printk(K_DEBUG, "!evalue\n");

		/* [12:8] (RX_50_code) => RG_SSUSB_IEXT_RX_IMPSEL[4:0] */
		evalue = (get_devinfo_with_index(107) & (0x1f << 8)) >> 8;
		if (evalue) {
			os_printk(K_INFO, "apply efuse setting, rg_ssusb_rx_impsel=0x%x\n",
				evalue);
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_PHYD_IMPCAL1, RG_SSUSB_RX_IMPSEL_OFST,
				RG_SSUSB_RX_IMPSEL, evalue);
		} else
			os_printk(K_DEBUG, "!evalue\n");

		/* [4:0] (TX_50_code) => RG_SSUSB_IEXT_TX_IMPSEL[4:0] */
		/* don't care : 0-bit */
		evalue = (get_devinfo_with_index(107) & (0x1f << 0)) >> 0;
		if (evalue) {
			os_printk(K_INFO, "apply efuse setting, rg_ssusb_tx_impsel=0x%x\n",
				evalue);
			U3PhyWriteField32((phys_addr_t) (uintptr_t)
				U3D_PHYD_IMPCAL0, RG_SSUSB_TX_IMPSEL_OFST,
				RG_SSUSB_TX_IMPSEL, evalue);

		} else
			os_printk(K_DEBUG, "!evalue\n");
	}

	/* For host, disconnect threshold */
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_DISCTH_OFST, RG_USB20_DISCTH, 0xF);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
		RG_USB20_PHY_REV_6_OFST, RG_USB20_PHY_REV_6, usb20_phy_rev6);

#ifdef MTK_USB_PHY_TUNING
	mtk_usb_phy_tuning();
#endif

	usb_phy_tuning();

	/* USB PLL Force settings */
	usb20_pll_settings(false, false);

	/* special request from DE */
	{
		u32 val, offset;

		/* RG_SSUSB_TX_EIDLE_CM, 0x11F40B20[31:28] */
		/* 4'b1000, ssusb_USB30_PHYA_regmap_T12FF_TPHY */
		val = 0x8;
		offset = 0x20;
		U3PhyWriteField32(
			(phys_addr_t) (uintptr_t)
			((u3_sif2_base + 0xb00) + offset), 28,
			(0xf<<28), val);

		/* rg_ssusb_cdr_bir_ltr, 0x11F4095C[20:16]  5'b01101 */
		/* ssusb_USB30_PHYD_regmap_T12FF_TPHY */
		val = 0xd;
		offset = 0x5c;
		U3PhyWriteField32(
			(phys_addr_t) (uintptr_t)
			((u3_sif2_base + 0x900) + offset), 16,
			(0x1f<<16), val);
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
reg_done:
#endif
	os_printk(K_INFO, "%s-\n", __func__);
}

/*
 * This function is to solve the condition described below.
 * The system boot has 3 situations.
 * 1. Booting without cable, so connection work called by musb_gadget_start()
 *    would turn off pwr/clk by musb_stop(). [REF CNT = 0]
 * 2. Booting with normal cable, the pwr/clk has already turned on
 *    at initial stage. and also set the flag (musb->is_clk_on=1).
 *    So musb_start() would not turn on again. [REF CNT = 1]
 * 3. Booting with OTG cable, the pwr/clk would be turned on
 *    by host one more time.[REF CNT=2]
 *    So device should turn off pwr/clk which are turned on
 *    during the initial stage.
 *    However, does _NOT_ touch the PHY registers. So we need this fake
 *    function to keep the REF CNT correct.
 *    NOT FOR TURN OFF PWR/CLK.
 */
void usb_fake_powerdown(unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (clk_on) {
		/* f_fusb30_ck:125MHz */
		usb_enable_clock(false);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

#ifdef CONFIG_USBIF_COMPLIANCE
static bool charger_det_en = true;

void Charger_Detect_En(bool enable)
{
	charger_det_en = enable;
}
#endif


/* BC1.2 */
void Charger_Detect_Init(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

#ifdef U3_COMPLIANCE
	os_printk(K_INFO, "%s, U3_COMPLIANCE, SKIP\n", __func__);
	return;
#endif

	if (mu3d_force_on || !_mu3d_musb) {
		os_printk(K_INFO, "%s-, SKIP\n", __func__);
		return;
	}

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		/* turn on USB reference clock. */
		usb_enable_clock(true);

		/* wait 50 usec. */
		udelay(250);

		/* RG_USB20_BC11_SW_EN = 1'b1 */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
			RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 1);

		udelay(1);

		/* 4 14. turn off internal 48Mhz PLL. */
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_INFO, "%s do not init detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_INFO, "%s-\n", __func__);
}

void Charger_Detect_Release(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

#ifdef U3_COMPLIANCE
	os_printk(K_INFO, "%s, U3_COMPLIANCE, SKIP\n", __func__);
	return;
#endif

	if (mu3d_force_on || !_mu3d_musb) {
		os_printk(K_INFO, "%s-, SKIP\n", __func__);
		return;
	}

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		/* turn on USB reference clock. */
		usb_enable_clock(true);

		/* wait 50 usec. */
		udelay(250);

		/* RG_USB20_BC11_SW_EN = 1'b0 */
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6,
			RG_USB20_BC11_SW_EN_OFST, RG_USB20_BC11_SW_EN, 0);

		udelay(1);

		/* 4 14. turn off internal 48Mhz PLL. */
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_DEBUG, "%s do not release detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_INFO, "%s-\n", __func__);
}

static int mt_usb_dts_probe(struct platform_device *pdev)
{
	int retval = 0;

	/* POWER */
	reg_vusb = regulator_get(&pdev->dev, "vusb");
	if (!IS_ERR(reg_vusb)) {
		retval = regulator_enable(reg_vusb);
		if (retval < 0) {
			pr_notice("regulator_enable vusb failed: %d\n", retval);
			regulator_put(reg_vusb);
		}
	} else {
		pr_notice("regulator_get vusb failed\n");
		reg_vusb = NULL;
	}

	reg_va09 = regulator_get(&pdev->dev, "va09");
	if (IS_ERR(reg_va09)) {
		pr_notice("regulator_get va09 failed\n");
		reg_va09 = NULL;
	}

	ssusb_clk = devm_clk_get(&pdev->dev, "ssusb_clk");
	if (IS_ERR(ssusb_clk)) {
		pr_notice("ssusb_clk get ssusb_clk fail\n");
	} else {
		retval = clk_prepare(ssusb_clk);
		if (retval == 0)
			pr_debug("ssusb_clk<%p> prepare done\n", ssusb_clk);
		else
			pr_notice("ssusb_clk prepare fail\n");
	}

	sys_ck = devm_clk_get(&pdev->dev, "sys_ck");
	if (IS_ERR(sys_ck)) {
		pr_notice("sys_ck get sys_ck fail\n");
	} else {
		retval = clk_prepare(sys_ck);
		if (retval == 0)
			pr_debug("sys_ck<%p> prepare done\n", sys_ck);
		else
			pr_notice("sys_ck prepare fail\n");
	}

	usb20_phy_rev6 = 1;
	pr_notice("%s, usb20_phy_rev6 to %d\n", __func__, usb20_phy_rev6);

	return retval;
}

static int mt_usb_dts_remove(struct platform_device *pdev)
{
	if (!IS_ERR(ssusb_clk))
		clk_unprepare(ssusb_clk);

	if (!IS_ERR(sys_ck))
		clk_unprepare(sys_ck);

	/* POWER */
	if (reg_vusb)
		regulator_put(reg_vusb);
	if (reg_va09)
		regulator_put(reg_va09);

	return 0;
}


static const struct of_device_id apusb_of_ids[] = {
	{.compatible = "mediatek,usb3_phy",},
	{},
};

MODULE_DEVICE_TABLE(of, apusb_of_ids);

static struct platform_driver mt_usb_dts_driver = {
	.remove = mt_usb_dts_remove,
	.probe = mt_usb_dts_probe,
	.driver = {
		   .name = "mt_dts_mu3phy",
		   .of_match_table = apusb_of_ids,
		   },
};
MODULE_DESCRIPTION("mtu3phy MUSB PHY Layer");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mt_usb_dts_driver);

void enable_ipsleep_wakeup(void)
{
	/* TODO */
}
void disable_ipsleep_wakeup(void)
{
	/* TODO */
}
#endif
