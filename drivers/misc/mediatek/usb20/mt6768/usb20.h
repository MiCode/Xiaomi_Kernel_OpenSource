/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __USB20_H__
#define __USB20_H__

#ifdef CONFIG_FPGA_EARLY_PORTING
#define FPGA_PLATFORM
#endif

#include <linux/interrupt.h>
#include <musb.h>

struct mt_usb_work {
	struct delayed_work dwork;
	int ops;
};

/* ToDo: should be moved to glue */
extern struct musb *mtk_musb;
extern struct musb *musb;

struct mt_usb_glue {
	struct device *dev;
	struct platform_device *musb_pdev;
	struct musb *mtk_musb;
	/* common power & clock */
	struct clk *musb_clk;
	struct clk *musb_clk_top_sel;
	struct clk *musb_clk_univpll3_d4;
#ifdef CONFIG_PHY_MTK_TPHY
	struct platform_device *usb_phy;
	struct phy *phy;
	struct usb_phy *xceiv;
	enum phy_mode phy_mode;
#endif
#ifdef CONFIG_MTK_MUSB_DUAL_ROLE
	struct otg_switch_mtk otg_sx;
#endif
};

extern struct mt_usb_glue *glue;

#define glue_to_musb(g)         platform_get_drvdata(g->musb)

extern int kernel_init_done;

extern unsigned int upmu_get_rgs_chrdet(void);
extern bool upmu_is_chr_det(void);

extern enum charger_type mt_charger_type_detection(void);
extern void BATTERY_SetUSBState(int usb_state);
extern void upmu_interrupt_chrdet_int_en(unsigned int val);

/* specific USB fuctnion */
enum CABLE_MODE {
	CABLE_MODE_CHRG_ONLY = 0,
	CABLE_MODE_NORMAL,
	CABLE_MODE_HOST_ONLY,
	CABLE_MODE_MAX
};

enum USB_CLK_STATE {
	NO_CHANGE = 0,
	ON_TO_OFF,
	OFF_TO_ON,
};

/* specific USB operation */
enum CONNECTION_OPS {
	CONNECTION_OPS_DISC = 0,
	CONNECTION_OPS_CHECK,
	CONNECTION_OPS_CONN
};

enum VBUS_OPS {
	VBUS_OPS_OFF = 0,
	VBUS_OPS_ON
};

#ifdef CONFIG_MTK_UART_USB_SWITCH
enum PORT_MODE {
	PORT_MODE_USB = 0,
	PORT_MODE_UART,
	PORT_MODE_MAX
};

extern bool usb_phy_check_in_uart_mode(void);
extern void usb_phy_switch_to_usb(void);
extern void usb_phy_switch_to_uart(void);
#endif

#ifdef FPGA_PLATFORM
extern void USB_PHY_Write_Register8(u8 var, u8 addr);
extern u8 USB_PHY_Read_Register8(u8 addr);
#endif

#ifdef CONFIG_MTK_UART_USB_SWITCH

#define RG_GPIO_SELECT (0x600)
#define GPIO_SEL_OFFSET (4)
#define GPIO_SEL_MASK (0x7 << GPIO_SEL_OFFSET)
#define GPIO_SEL_UART0 (0x1 << GPIO_SEL_OFFSET)
#define GPIO_SEL_UART1 (0x2 << GPIO_SEL_OFFSET)
#define GET_GPIO_SEL_VAL(x) ((x & GPIO_SEL_MASK) >> GPIO_SEL_OFFSET)

extern void __iomem *ap_gpio_base;
extern bool in_uart_mode;
#endif
extern int usb20_phy_init_debugfs(void);
extern enum charger_type mt_get_charger_type(void);
#ifndef CONFIG_FPGA_EARLY_PORTING
#include <upmu_common.h>
#endif
#define PHY_IDLE_MODE       0
#define PHY_DEV_ACTIVE      1
#define PHY_HOST_ACTIVE     2
void set_usb_phy_mode(int mode);
#ifdef CONFIG_USB_MTK_OTG
extern void mt_usb_otg_init(struct musb *musb);
extern void mt_usb_otg_exit(struct musb *musb);
extern int mt_usb_get_vbus_status(struct musb *musb);
extern void mt_usb_host_connect(int delay);
extern void mt_usb_host_disconnect(int delay);
extern void mt_usb_host_connect(int delay);
extern void mt_usb_host_disconnect(int delay);
#endif
extern void musb_platform_reset(struct musb *musb);
extern bool usb_enable_clock(bool enable);
extern bool usb_prepare_clock(bool enable);
extern void usb_prepare_enable_clock(bool enable);

/* usb host mode wakeup */
#define USB_WAKEUP_DEC_CON1	0x404
#define USB1_CDEN		BIT(0)
#define USB1_CDDEBOUNCE(x)	(((x) & 0xf) << 1)
#endif
