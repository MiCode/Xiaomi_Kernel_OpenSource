/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/interrupt.h>

#include <linux/phy/mediatek/mtk_usb_phy.h>
//#include "mtk_spm_resource_req.h"
//#include "mtk_idle.h"
#include "mu3d_hal_osal.h"
#include "musb_core.h"
#include "mtk-ssusb-hal.h"

static struct phy *mtk_phy;
static void __iomem *infra_ao_base;

static void u3phywrite32(u32 reg, int mode, int offset, int mask, int value)
{

	int cur_value;
	int new_value;

	cur_value = usb_mtkphy_io_read(mtk_phy, reg);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);
	usb_mtkphy_io_write(mtk_phy, new_value, reg);

}
#if 0
static int dpidle_status = USB_DPIDLE_ALLOWED;
static DEFINE_SPINLOCK(usb_hal_dpidle_lock);
#define DPIDLE_TIMER_INTERVAL_MS 30
static void issue_dpidle_timer(void);

static int usbaudio_idle_notify_call(struct notifier_block *nfb,
				unsigned long id,
				void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
		if (dpidle_status == USB_DPIDLE_AUDIO_SRAM) {
			writel(readl(infra_ao_base + 0x78) | (0x1<<1), infra_ao_base + 0x78);
			writel(readl(infra_ao_base + 0x78) & ~(0x1<<0), infra_ao_base + 0x78);
			writel(readl(infra_ao_base + 0x78) | (0x1<<6), infra_ao_base + 0x78);
			writel(readl(infra_ao_base + 0x78) | (0x1<<0), infra_ao_base + 0x78);
			writel(readl(infra_ao_base + 0x78) & ~(0x1<<1), infra_ao_base + 0x78);
		}
		break;
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
		writel(readl(infra_ao_base + 0x78) | (0x1<<1), infra_ao_base + 0x78);
		writel(readl(infra_ao_base + 0x78) & ~(0x1<<0), infra_ao_base + 0x78);
		writel(readl(infra_ao_base + 0x78) & ~(0x1<<6), infra_ao_base + 0x78);
		writel(readl(infra_ao_base + 0x78) | (0x1<<0), infra_ao_base + 0x78);
		writel(readl(infra_ao_base + 0x78) & ~(0x1<<1), infra_ao_base + 0x78);
		break;
	case NOTIFY_SOIDLE3_ENTER:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		/* do nothing */
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block usbaudio_idle_nfb = {
	.notifier_call = usbaudio_idle_notify_call,
};

static void dpidle_timer_wakeup_func(unsigned long data)
{
	struct timer_list *timer = (struct timer_list *)data;

	{
		static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 1);
		static int skip_cnt;

		if (__ratelimit(&ratelimit)) {
			os_printk(K_INFO, "dpidle_timer<%p> alive, skip_cnt<%d>\n", timer, skip_cnt);
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

	spin_lock_irqsave(&usb_hal_dpidle_lock, flags);

	/* update dpidle_status */
	dpidle_status = mode;
	usb_audio_req(false);

	switch (mode) {

	case USB_DPIDLE_ALLOWED:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, 0);
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
				os_printk(K_INFO, "USB_DPIDLE_SRAM, skip_cnt<%d>\n", skip_cnt);
				skip_cnt = 0;
			} else
				skip_cnt++;
		}
		break;
	case USB_DPIDLE_AUDIO_SRAM:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, 0);
		usb_audio_req(true);
		{
			static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);
			static int skip_cnt;

			if (__ratelimit(&ratelimit)) {
				os_printk(K_INFO, "USB_DPIDLE_AUDIO_SRAM, skip_cnt<%d>\n", skip_cnt);
				skip_cnt = 0;
			} else
				skip_cnt++;
		}
		break;
	case USB_DPIDLE_TIMER:
		spm_resource_req(SPM_RESOURCE_USER_SSUSB, SPM_RESOURCE_CK_26M);
		os_printk(K_INFO, "USB_DPIDLE_TIMER\n");
		issue_dpidle_timer();
		break;
	default:

		os_printk(K_WARNIN, "[ERROR] Are you kidding!?!?\n");
		break;
	}

	spin_unlock_irqrestore(&usb_hal_dpidle_lock, flags);
}
#endif
void usb20_rev6_setting(int value, bool is_update)
{
	u3phywrite32(0x18, 0, 30, (0x3 << 30), value);
}

#ifdef CONFIG_MTK_UART_USB_SWITCH
bool in_uart_mode;
static void __iomem *ap_uart0_base;
static u32 usb2uart_reg, usb2uart_offset;

void uart_usb_switch_dump_register(void)
{
}

bool usb_phy_check_in_uart_mode(void)
{
	if (usb_mtkphy_check_in_uart_mode(mtk_phy) > 0)
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	usb_mtkphy_switch_to_uart(mtk_phy);
	/* GPIO Selection */
	if (ap_uart0_base) {
		DRV_WriteReg32(ap_uart0_base + usb2uart_reg,
			(DRV_Reg32(ap_uart0_base + usb2uart_reg) | (1 << usb2uart_offset)));
	}

	in_uart_mode = true;
}


void usb_phy_switch_to_usb(void)
{
	if (ap_uart0_base) {
		DRV_WriteReg32(ap_uart0_base + usb2uart_reg,
			(DRV_Reg32(ap_uart0_base + usb2uart_reg) & ~(1 << usb2uart_offset)));
	}

	usb_mtkphy_switch_to_usb(mtk_phy);
	in_uart_mode = false;
}

u32 usb_phy_get_uart_path(void)
{
	if (ap_uart0_base)
		return DRV_Reg32(ap_uart0_base + usb2uart_reg);
	else
		return 0;
}


static int mt_usb_uart_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;

	if (of_property_read_u32(node, "usb2uart_reg", (u32 *) &usb2uart_reg))
		return -EINVAL;

	if (of_property_read_u32(node, "usb2uart_offset", (u32 *) &usb2uart_offset))
		return -EINVAL;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	ap_uart0_base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(ap_uart0_base)) {
		dev_info(dev, "failed to remap usb2uart regs\n");
		return PTR_ERR(ap_uart0_base);
	}

	return 0;
}

static int mt_usb_uart_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id usb2uart_of_ids[] = {
	{.compatible = "mediatek,usb2uart",},
	{},
};

MODULE_DEVICE_TABLE(of, usb2uart_of_ids);

static struct platform_driver usb2uart_dts_driver = {
	.remove = mt_usb_uart_remove,
	.probe = mt_usb_uart_probe,
	.driver = {
		   .name = "mt_dts_usb2uart",
		   .of_match_table = usb2uart_of_ids,
		   },
};
MODULE_DESCRIPTION("mtud usb2uart Driver");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(usb2uart_dts_driver);
#endif


#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT
bool u3_loop_back_test(void)
{
	int ret;

	phy_power_on(mtk_phy);
	/*SSUSB_IP_SW_RST = 0*/
	writel(0x00031000, U3D_SSUSB_IP_PW_CTRL0);
	/*SSUSB_IP_HOST_PDN = 0*/
	writel(0x00000000, U3D_SSUSB_IP_PW_CTRL1);
	/*SSUSB_IP_DEV_PDN = 0*/
	writel(0x00000000, U3D_SSUSB_IP_PW_CTRL2);
	/*SSUSB_IP_PCIE_PDN = 0*/
	writel(0x00000000, U3D_SSUSB_IP_PW_CTRL3);
	/*SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
	writel(0x0000000C, U3D_SSUSB_U3_CTRL_0P);
	mdelay(10);
	if (usb_mtkphy_u3_loop_back_test(mtk_phy) > 0)
		ret = true;
	else
		ret = false;
	phy_power_off(mtk_phy);

	return ret;
}
#endif

#ifdef CONFIG_MTK_SIB_USB_SWITCH
#include <linux/wakelock.h>
static struct wake_lock sib_wakelock;
bool in_sib_mode;

void usb_phy_sib_enable_switch(bool enable)
{
	if (mtk_phy) {
		if (enable & !in_sib_mode) {
			phy_power_on(mtk_phy);
			/*SSUSB_IP_SW_RST = 0*/
			writel(0x00031000, U3D_SSUSB_IP_PW_CTRL0);
			/*SSUSB_IP_HOST_PDN = 0*/
			writel(0x00000000, U3D_SSUSB_IP_PW_CTRL1);
			/*SSUSB_IP_DEV_PDN = 0*/
			writel(0x00000000, U3D_SSUSB_IP_PW_CTRL2);
			/*SSUSB_IP_PCIE_PDN = 0*/
			writel(0x00000000, U3D_SSUSB_IP_PW_CTRL3);
			/*SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
			writel(0x0000000C, U3D_SSUSB_U3_CTRL_0P);
			mdelay(10);

			usb_mtkphy_sib_enable_switch(mtk_phy, true);
			if (!wake_lock_active(&sib_wakelock))
				wake_lock(&sib_wakelock);
			in_sib_mode = 1;
		} else if (!enable && in_sib_mode) {
			/*SSUSB_U3_PORT_DIS/SSUSB_U3_PORT_PDN = 0*/
			writel(0x0000000F, U3D_SSUSB_U3_CTRL_0P);
			/*SSUSB_IP_PCIE_PDN = 0*/
			writel(0x00000001, U3D_SSUSB_IP_PW_CTRL3);
			/*SSUSB_IP_DEV_PDN = 0*/
			writel(0x00000001, U3D_SSUSB_IP_PW_CTRL2);
			/*SSUSB_IP_HOST_PDN = 0*/
			writel(0x00000001, U3D_SSUSB_IP_PW_CTRL1);
			/*SSUSB_IP_SW_RST = 0*/
			writel(0x00031001, U3D_SSUSB_IP_PW_CTRL0);
			mdelay(10);

			usb_mtkphy_sib_enable_switch(mtk_phy, false);
			phy_power_off(mtk_phy);
			if (wake_lock_active(&sib_wakelock))
				wake_unlock(&sib_wakelock);
			in_sib_mode	= 0;
		}
	}
}

bool usb_phy_sib_enable_switch_status(void)
{
	u32 value;

	if (mtk_phy) {
		value = usb_mtkphy_io_read(mtk_phy, 0x300);
		if (value == 0x62910008)
			return true;
		else
			return false;
	} else
		return false;
}
#endif


#ifdef CONFIG_MTK_USB2JTAG_SUPPORT
int usb2jtag_usb_init(void)
{
	u32 temp;

	if (mtk_phy) {
		/* rg_usb20_gpio_ctl: bit[9] = 1 */
		temp = usb_mtkphy_io_read(mtk_phy, 0x20);
		usb_mtkphy_io_write(mtk_phy, temp | (1 << 9), 0x20);

		/* RG_USB20_BC11_SW_EN: bit[23] = 0 */
		temp = usb_mtkphy_io_read(mtk_phy, 0x18);
		usb_mtkphy_io_write(mtk_phy, temp & ~(1 << 23), 0x18);

		/* RG_USB20_BGR_EN: bit[0] = 1 */
		temp = usb_mtkphy_io_read(mtk_phy, 0x0);
		usb_mtkphy_io_write(mtk_phy, temp | (1 << 0), 0x0);

		/* rg_sifslv_mac_bandgap_en: bit[17] = 0 */
		temp = usb_mtkphy_io_read(mtk_phy, 0x8);
		usb_mtkphy_io_write(mtk_phy, temp & ~(1 << 17), 0x8);

		/* wait stable */
		mdelay(1);
	}
	return 0;
}
#endif

#ifdef CONFIG_USBIF_COMPLIANCE
static bool charger_det_en = true;

void Charger_Detect_En(bool enable)
{
	charger_det_en = enable;
}
#endif


#ifdef NEVER
/* BC1.2 */
void Charger_Detect_Init(void)
{
	os_printk(K_INFO, "%s+\n", __func__);
	if (mu3d_force_on) {
		os_printk(K_INFO, "%s-, SKIP\n", __func__);
		return;
	}

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		if (mtk_phy)
			usb_mtkphy_switch_to_bc11(mtk_phy, true);

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

	if (mu3d_force_on) {
		os_printk(K_INFO, "%s-, SKIP\n", __func__);
		return;
	}

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		if (mtk_phy)
			usb_mtkphy_switch_to_bc11(mtk_phy, false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_DEBUG, "%s do not release detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_INFO, "%s-\n", __func__);
}
#endif /* NEVER */

void init_phy_hal(struct phy *phy)
{
	struct device_node *node;
	struct device_node *phy_node = phy->dev.of_node;

	mtk_phy = phy;
	if (of_device_is_compatible(phy_node, "mediatek,mt6758-phy")) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
		if (node) {
			infra_ao_base = of_iomap(node, 0);
			//if (infra_ao_base)
			//	mtk_idle_notifier_register(&usbaudio_idle_nfb);
		}
	}

#ifdef CONFIG_MTK_SIB_USB_SWITCH
	wake_lock_init(&sib_wakelock, "SIB.lock");
#endif
}

static int usb_ipsleep_irqnum;
static int usb_ipsleep_init;

void mask_ipsleep(void)
{
	disable_irq(usb_ipsleep_irqnum);
}
void unmask_ipsleep(void)
{
	enable_irq(usb_ipsleep_irqnum);
}
void enable_ipsleep_wakeup(void)
{
	if (usb_ipsleep_init)
		unmask_ipsleep();
}
void disable_ipsleep_wakeup(void)
{
}

static irqreturn_t musb_ipsleep_eint_iddig_isr(int irqnum, void *data)
{
	disable_irq_nosync(irqnum);
	os_printk(K_ALET, "usb_ipsleep\n");
	return IRQ_HANDLED;
}
static int mtk_usb_ipsleep_eint_irq_en(struct platform_device *pdev)
{
	int retval = 0;

	retval = request_irq(usb_ipsleep_irqnum, musb_ipsleep_eint_iddig_isr, IRQF_TRIGGER_NONE, "usbcd_eint",
					pdev);
	if (retval != 0) {
		os_printk(K_ERR, "usbcd request_irq fail, ret %d, irqnum %d!!!\n", retval,
			 usb_ipsleep_irqnum);
	} else {
		enable_irq_wake(usb_ipsleep_irqnum);
	}
	return retval;
}
static int mt_usb_ipsleep_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	usb_ipsleep_irqnum = irq_of_parse_and_map(node, 0);
	if (usb_ipsleep_irqnum < 0)
		return -ENODEV;
	retval = mtk_usb_ipsleep_eint_irq_en(pdev);
	if (retval != 0)
		goto irqfail;
	usb_ipsleep_init = 1;
irqfail:
	return retval;
}
static int mt_usb_ipsleep_remove(struct platform_device *pdev)
{
	free_irq(usb_ipsleep_irqnum, pdev);
	return 0;
}
static const struct of_device_id usb_ipsleep_of_match[] = {
	{.compatible = "mediatek,usb_ipsleep"},
	{},
};
MODULE_DEVICE_TABLE(of, usb_ipsleep_of_match);
static struct platform_driver mt_usb_ipsleep_driver = {
	.remove = mt_usb_ipsleep_remove,
	.probe = mt_usb_ipsleep_probe,
	.driver = {
		   .name = "usb_ipsleep",
		   .of_match_table = usb_ipsleep_of_match,
		   },
};
MODULE_DESCRIPTION("musb ipsleep eint");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mt_usb_ipsleep_driver);

