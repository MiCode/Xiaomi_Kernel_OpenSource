/*
 * MUSB OTG driver core code
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Inventra (Multipoint) Dual-Role Controller Driver for Linux.
 *
 * This consists of a Host Controller Driver (HCD) and a peripheral
 * controller driver implementing the "Gadget" API; OTG support is
 * in the works.  These are normal Linux-USB controller drivers which
 * use IRQs and have no dedicated thread.
 *
 * This version of the driver has only been used with products from
 * Texas Instruments.  Those products integrate the Inventra logic
 * with other DMA, IRQ, and bus modules, as well as other logic that
 * needs to be reflected in this driver.
 *
 *
 * NOTE:  the original Mentor code here was pretty much a collection
 * of mechanisms that don't seem to have been fully integrated/working
 * for *any* Linux kernel version.  This version aims at Linux 2.6.now,
 * Key open issues include:
 *
 *  - Lack of host-side transaction scheduling, for all transfer types.
 *    The hardware doesn't do it; instead, software must.
 *
 *    This is not an issue for OTG devices that don't support external
 *    hubs, but for more "normal" USB hosts it's a user issue that the
 *    "multipoint" support doesn't scale in the expected ways.  That
 *    includes DaVinci EVM in a common non-OTG mode.
 *
 *      * Control and bulk use dedicated endpoints, and there's as
 *        yet no mechanism to either (a) reclaim the hardware when
 *        peripherals are NAKing, which gets complicated with bulk
 *        endpoints, or (b) use more than a single bulk endpoint in
 *        each direction.
 *
 *        RESULT:  one device may be perceived as blocking another one.
 *
 *      * Interrupt and isochronous will dynamically allocate endpoint
 *        hardware, but (a) there's no record keeping for bandwidth;
 *        (b) in the common case that few endpoints are available, there
 *        is no mechanism to reuse endpoints to talk to multiple devices.
 *
 *        RESULT:  At one extreme, bandwidth can be overcommitted in
 *        some hardware configurations, no faults will be reported.
 *        At the other extreme, the bandwidth capabilities which do
 *        exist tend to be severely undercommitted.  You can't yet hook
 *        up both a keyboard and a mouse to an external USB hub.
 */
/* Sanity CR check in */
/*
 * This gets many kinds of configuration information:
 *	- Kconfig for everything user-configurable
 *	- platform_device for addressing, irq, and platform_data
 *	- platform_data is mostly for board-specific informarion
 *	  (plus recentrly, SOC or family details)
 *
 * Most of the conditional compilation will (someday) vanish.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/prefetch.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/wakelock.h>

#include <mu3phy/mtk-phy.h>
#include <mu3phy/mtk-phy-asic.h>	/* for fc_iddig, rm later */
#include "musb_core.h"
/* #include "mu3d_hal_osal.h" */
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "mu3d_hal_qmu_drv.h"
#include "ssusb_io.h"

/* #define TA_WAIT_BCON(m) max_t(int, (m)->a_wait_bcon, OTG_TIME_A_WAIT_BCON) */


#define DRIVER_AUTHOR "Mentor Graphics, Texas Instruments, Nokia"
#define DRIVER_DESC "Inventra Dual-Role USB Controller Driver"

#define MUSB_VERSION "6.0"

#define DRIVER_INFO DRIVER_DESC ", v" MUSB_VERSION

const char musb_driver_name[] = MUSB_DRIVER_NAME;

struct musb *_mu3d_musb;

u32 debug_level = K_ALET | K_CRIT | K_ERR | K_WARNIN | K_NOTICE;

module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug Print Log Lvl");

MODULE_DESCRIPTION(DRIVER_INFO);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" MUSB_DRIVER_NAME);

#define U3D_FIFO_START_ADDRESS 0


static void print_buf(char *verb, const char *buf, int len)
{
}

/*
 * Load an endpoint's FIFO
 */
void musb_write_fifo(struct musb_hw_ep *hw_ep, u16 len, const u8 *src)
{
	unsigned int residue;
	unsigned int temp;
	void __iomem *fifo = hw_ep->fifo;	/* (void __iomem *)USB_FIFO(hw_ep->epnum); */

	mu3d_dbg(K_DEBUG, "%s epnum=%d, len=%d, buf=%p\n", __func__, hw_ep->epnum, len, src);

	residue = len;
	print_buf("write-fifo", src, len);

	while (residue > 0) {

		if (residue == 1) {
			temp = ((*src) & 0xFF);
			writeb(temp, fifo);
			src += 1;
			residue -= 1;
		} else if (residue == 2) {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00);
			writew(temp, fifo);
			src += 2;
			residue -= 2;
		} else if (residue == 3) {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00);
			writew(temp, fifo);
			src += 2;

			temp = ((*src) & 0xFF);
			writeb(temp, fifo);
			src += 1;
			residue -= 3;
		} else {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00) +
			    (((*(src + 2)) << 16) & 0xFF0000) + (((*(src + 3)) << 24) & 0xFF000000);
			writel(temp, fifo);
			src += 4;
			residue -= 4;
		}
	}
}

/*
 * Unload an endpoint's FIFO
 */
void musb_read_fifo(struct musb_hw_ep *hw_ep, u16 len, u8 *dst)
{
	u16 residue;
	unsigned int temp;
	u8 *dst_bg = dst;
	void __iomem *fifo = hw_ep->fifo;	/* (void __iomem *)USB_FIFO(hw_ep->epnum); */

	mu3d_dbg(K_DEBUG, "%s %cX ep%d fifo %p count %d buf %p\n",
		 __func__, 'R', hw_ep->epnum, fifo, len, dst);

	residue = len;


	while (residue > 0) {

		temp = readl(fifo);

		/*Store the first byte */
		*dst = temp & 0xFF;

		/*Store the 2nd byte, If have */
		if (residue > 1)
			*(dst + 1) = (temp >> 8) & 0xFF;

		/*Store the 3rd byte, If have */
		if (residue > 2)
			*(dst + 2) = (temp >> 16) & 0xFF;

		/*Store the 4th byte, If have */
		if (residue > 3)
			*(dst + 3) = (temp >> 24) & 0xFF;

		if (residue > 4) {
			dst = dst + 4;
			residue = residue - 4;
		} else {
			residue = 0;
		}
	}
	print_buf("read-fifo", dst_bg, len);

}



/*-------------------------------------------------------------------------*/

/* for high speed test mode; see USB 2.0 spec 7.1.20 */
static const u8 musb_test_packet[53] = {
	/* implicit SYNC then DATA0 to start */

	/* JKJKJKJK x9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* JJKKJJKK x8 */
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	/* JJJJKKKK x8 */
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	/* JJJJJJJKKKKKKK x8 */
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* JJJJJJJK x8 */
	0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
	/* JKKKKKKK x10, JK */
	0xfc, 0x7e, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x7e
	    /* implicit CRC16 then EOP to end */
};

void musb_load_testpacket(struct musb *musb)
{
	u32 maxp;

	maxp = musb->endpoints->max_packet_sz_tx;
	mu3d_hal_write_fifo(&musb->endpoints[0], 0, sizeof(musb_test_packet),
			    (u8 *) musb_test_packet, maxp);
}

/*-------------------------------------------------------------------------*/


/*
 * Stops the HNP transition. Caller must take care of locking.
 */
/* void musb_hnp_stop(struct musb *musb){} */

/*
 * Interrupt Service Routine to record USB "global" interrupts.
 * Since these do not happen often and signify things of
 * paramount importance, it seems OK to check them individually;
 * the order of the tests is specified in the manual
 *
 * @param musb instance pointer
 * @param int_usb register contents
 * @param devctl
 * @param power
 */

static irqreturn_t musb_stage0_irq(struct musb *musb, u32 int_usb, u32 devctl, u32 power)
{
	struct usb_otg *otg = musb->xceiv->otg;
	irqreturn_t handled = IRQ_NONE;

	dev_notice(musb->controller, "<== Power=%02x, DevCtl=%02x, int_usb=0x%x\n", power, devctl,
		   int_usb);
	/* in host mode, the peripheral may issue remote wakeup.
	 * in peripheral mode, the host may resume the link.
	 * spurious RESUME irqs happen too, paired with SUSPEND.
	 */
	if (int_usb & RESUME_INTR) {
		handled = IRQ_HANDLED;
		dev_notice(musb->controller, "RESUME (%s)\n",
			   usb_otg_state_string(musb->xceiv->state));

		/* We implement device mode only. */
		switch (musb->xceiv->state) {
		case OTG_STATE_B_PERIPHERAL:
			/* disconnect while suspended?  we may
			 * not get a disconnect irq...
			 */
			if ((devctl & VBUS) != USB_DEVCTL_VBUSVALID) {
				musb->int_usb |= DISCONN_INTR;
				musb->int_usb &= ~SUSPEND_INTR;
				break;
			}
			musb_g_resume(musb);
			break;
		case OTG_STATE_B_IDLE:
			musb->int_usb &= ~SUSPEND_INTR;
			break;
		default:
			mu3d_dbg(K_ERR, "bogus %s RESUME (%s)\n",
				 "peripheral", usb_otg_state_string(musb->xceiv->state));
		}
	}

	/* see manual for the order of the tests */
	if (int_usb & SESSION_REQ_INTR) {	/* no use anymore is ssusb */

		handled = IRQ_HANDLED;
	}

	if (int_usb & VBUSERR_INTR) {	/* only for a-device, but now we only work as b-peripheral */

		handled = IRQ_HANDLED;
	}

	if (int_usb & SUSPEND_INTR) {
		dev_notice(musb->controller, "SUSPEND (%s) devctl %02x power %02x\n",
			   usb_otg_state_string(musb->xceiv->state), devctl, power);
		handled = IRQ_HANDLED;

		switch (musb->xceiv->state) {

		case OTG_STATE_B_IDLE:
			if (!musb->is_active)
				break;
		case OTG_STATE_B_PERIPHERAL:
			musb_g_suspend(musb);
			musb->is_active = is_otg_enabled(musb) && otg->gadget->b_hnp_enable;
			/* if (musb->is_active) { */
			/* musb->xceiv->state = OTG_STATE_B_WAIT_ACON; */
			/* dev_dbg(musb->controller, "HNP: Setting timer for b_ase0_brst\n"); */
			/* mod_timer(&musb->otg_timer, jiffies */
			/* + msecs_to_jiffies(OTG_TIME_B_ASE0_BRST)); */
			/* } */
			break;
		default:
			/* "should not happen" */
			musb->is_active = 0;
			dev_dbg(musb->controller, "REVISIT: SUSPEND as %s\n",
				usb_otg_state_string(musb->xceiv->state));
			break;
		}
	}
#if 0				/* host only ,check it */
	if (int_usb & CONN_INTR) {
		dev_notice(musb->controller, "CONNECT (%s) devctl %02x\n",
			   usb_otg_state_string(musb->xceiv->state), devctl);
	}

	if (int_usb & DISCONN_INTR) {
		dev_notice(musb->controller, "DISCONNECT (%s), devctl %02x\n",
			   usb_otg_state_string(musb->xceiv->state), devctl);
		handled = IRQ_HANDLED;

		switch (musb->xceiv->state) {
		case OTG_STATE_B_PERIPHERAL:
		case OTG_STATE_B_IDLE:
			musb_g_disconnect(musb);
			break;
		default:
			WARNING("unhandled DISCONNECT transition (%s)\n",
				usb_otg_state_string(musb->xceiv->state));
			break;
		}
	}
#endif
	/* mentor saves a bit: bus reset and babble share the same irq.
	 * only host sees babble; only peripheral sees bus reset.
	 */
	if (int_usb & RESET_INTR) {
		handled = IRQ_HANDLED;

		dev_notice(musb->controller, "BUS RESET as %s\n",
			   usb_otg_state_string(musb->xceiv->state));
		mu3d_dbg(K_INFO, "BUS RESET\n");
		switch (musb->xceiv->state) {
		case OTG_STATE_B_IDLE:
			musb->xceiv->state = OTG_STATE_B_PERIPHERAL;
			/* FALLTHROUGH */
		case OTG_STATE_B_PERIPHERAL:
			musb_g_reset(musb);
			break;
		default:
			dev_dbg(musb->controller, "Unhandled BUS RESET as %s\n",
				usb_otg_state_string(musb->xceiv->state));
		}
	}

	queue_work(musb->wq, &musb->otg_event_work);

	return handled;
}

static void musb_restore_context(struct musb *musb);
static void musb_save_context(struct musb *musb);

static void print_regs(void __iomem *mbase, int offset, int len, char *name)
{
	int i;
	int length = ((len >> 2) + 3) >> 2;
	void __iomem *base = mbase + offset;

	mu3d_dbg(K_ERR, "%s(%d)- %s regs:\n", __func__, __LINE__, name);

	for (i = 0; i < length; i++) {
		mu3d_dbg(K_ERR, "%p: 0x%08X 0x%08X 0x%08X 0x%08X\n",
			 base, mu3d_readl(base, 0), mu3d_readl(base + 4, 0),
			 mu3d_readl(base + 8, 0), mu3d_readl(base + 12, 0));
		base = base + 16;
	}
	mu3d_dbg(K_ERR, "\n");
}

void print_mu3d_regs(struct musb *musb)
{
	print_regs(musb->mac_base, SSUSB_DEV_BASE, 0xd00, "SSUSB_DEV_BASE");
	print_regs(musb->sif_base, SSUSB_SIFSLV_IPPC_BASE, 0x100, "SSUSB_SIFSLV_IPPC_BASE");
	print_regs(musb->sif_base, SSUSB_SIFSLV_U2PHY_COM_BASE, 0x100,
		   "SSUSB_SIFSLV_U2PHY_COM_BASE");
}


/*-------------------------------------------------------------------------*/

void musb_dev_on_off(struct musb *musb, int is_on)
{
	mu3d_dbg(K_DEBUG, "%s\n", __func__);

	if (is_on) {
#ifdef SUPPORT_U3
		mu3d_hal_u3dev_en(musb);
#else
		mu3d_hal_u2dev_connect(musb);
#endif
	} else {
#ifdef SUPPORT_U3
		mu3d_hal_u3dev_dis(musb);
#endif
		mu3d_hal_u2dev_disconn(musb);
	}

	dev_notice(musb->controller, "gadget pullup D%s\n",
		   is_on ? "+ (soft-connect)" : "- (soft-disconnect)");
}


/*
* Program the HDRC to start (enable interrupts, dma, etc.).
*/
void musb_start(struct musb *musb)
{
	void __iomem *mbase = musb->mac_base;
	u8 devctl = (u8) mu3d_readl(mbase, U3D_DEVICE_CONTROL);

	dev_dbg(musb->controller, "<== devctl %02x\n", devctl);

	mu3d_dbg(K_INFO, "%s\n", __func__);

	/* otg//if (musb->is_clk_on == 0) { */
#ifndef CONFIG_MTK_FPGA
	/* Recovert PHY. And turn on CLK. */
	/* otg//u3phy->u3p_ops->usb_phy_recover(u3phy, musb->is_clk_on); */
	musb->is_clk_on = 1;

	/* USB 2.0 slew rate calibration */
	/* otg//u3phy->u3p_ops->u2_slew_rate_calibration(u3phy); */
#endif
	/* mu3d_setmsk(musb->sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST); */
	/* mdelay(2); */
	ssusb_power_restore(musb->ssusb);

	/* disable IP reset and power down, disable U2/U3 ip power down */
	mu3d_hal_ssusb_en(musb);

	/* reset U3D all dev module. */
	mu3d_hal_rst_dev(musb);

	musb_restore_context(musb);
	/* otg//} */

	/*Enable Level 1 interrupt (BMU, QMU, MAC3, DMA, MAC2, EPCTL) */
	mu3d_writel(mbase, U3D_LV1IESR, 0xFFFFFFFF);

	/* Initialize the default interrupts */
	mu3d_hal_system_intr_en(musb);

#ifdef USB_GADGET_SUPERSPEED
	/* HS/FS detected by HW */
	/* USB2.0 controller will negotiate for HS mode when the device is reset by the host */
	mu3d_setmsk(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
	/* Accept LGO_U1/U2 */
	mu3d_setmsk(mbase, U3D_LINK_POWER_CONTROL, SW_U1_ACCEPT_ENABLE | SW_U2_ACCEPT_ENABLE);
	/* device responses to u3_exit from host automatically */
	mu3d_clrmsk(mbase, U3D_LTSSM_CTRL, SOFT_U3_EXIT_EN);
#else
#ifdef USB_GADGET_DUALSPEED
	/* HS/FS detected by HW */
	mu3d_setmsk(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
#else
	/* FS only */
	mu3d_clrmsk(mbase, U3D_POWER_MANAGEMENT, HS_ENABLE);
#endif
	/* disable U3 port */
	mu3d_hal_u3dev_dis(musb);
#endif
	/* delay about 0.1us from detecting reset to send chirp-K */
	mu3d_clrmsk(mbase, U3D_LINK_RESET_INFO, WTCHRP);

	if (need_vbus_chg_int(musb))
		mu3d_clrmsk(mbase, U3D_MISC_CTRL, VBUS_FRC_EN | VBUS_ON);

	/* U2/U3 detected by HW */
	mu3d_writel(mbase, U3D_DEVICE_CONF, 0);
	/* OTG switch by iddig */
	ssusb_otg_plug_out(musb->ssusb);

	musb->is_active = 1;

	musb_platform_enable(musb);

	if (musb->softconnect)
		musb_dev_on_off(musb, 1);
	/* mu3d_hal_u3dev_en(musb); */

	/* print_mu3d_regs(musb); */
}


static void musb_generic_disable(struct musb *musb)
{
	/*Disable interrupts */
	mu3d_hal_initr_dis(musb);

	/*Clear all interrupt status */
	mu3d_hal_clear_intr(musb);
}

/*
 * 1. Disable U2 & U3 function
 * 2. Notify disconnect event to upper
 */
static void gadget_stop(struct musb *musb)
{
	/* Disable U2 detect */
	/* mu3d_hal_u3dev_dis(musb); */
	/* mu3d_hal_u2dev_disconn(musb); */
	musb_dev_on_off(musb, 0);

	/* notify gadget driver */
	if (musb->g.speed != USB_SPEED_UNKNOWN) {
		if (musb->gadget_driver && musb->gadget_driver->disconnect)
			musb->gadget_driver->disconnect(&musb->g);
		musb->g.speed = USB_SPEED_UNKNOWN;
	}
}

/*
 * Make the HDRC stop (disable interrupts, etc.);
 * reversible by musb_start
 * called on gadget driver unregister
 * with controller locked, irqs blocked
 * acts as a NOP unless some role activated the hardware
 */
void musb_stop(struct musb *musb)
{
	bool is_usb_cable = usb_cable_connected();
	mu3d_dbg(K_INFO, "musb_stop\n");

	/* stop IRQs, timers, ... */
	musb_platform_disable(musb);
	musb_generic_disable(musb);

	/*Added by M */
	gadget_stop(musb);
	musb->is_active = 0;
	/*Added by M */

	dev_dbg(musb->controller, "HDRC disabled\n");

	if ((musb->active_ep == 0) && !is_usb_cable)
		queue_work(musb->wq, &musb->suspend_work);

	/* Move to suspend work queue */
#ifdef NEVER
	/*
	 * Note: When reset the SSUSB IP, All MAC regs can _NOT_ be accessed and be reset to the default value.
	 * So save the MUST-SAVED reg in the context structure before set SSUSB_IP_SW_RST.
	 */
	musb_save_context(musb);

	/* Set SSUSB_IP_SW_RST to avoid power leakage */
	os_setmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);

#ifndef CONFIG_MTK_FPGA
	/* Let PHY enter savecurrent mode. And turn off CLK. */
	usb_phy_savecurrent(musb->is_clk_on);
	musb->is_clk_on = 0;
#endif
#endif				/* NEVER */

	/* FIXME
	 *  - mark host and/or peripheral drivers unusable/inactive
	 *  - disable DMA (and enable it in HdrcStart)
	 *  - make sure we can musb_start() after musb_stop(); with
	 *    OTG mode, gadget driver module rmmod/modprobe cycles that
	 *  - ...
	 */
	musb_platform_try_idle(musb, 0);
}

#if 0
static void musb_shutdown(struct platform_device *pdev)
{
	struct musb *musb = dev_to_musb(&pdev->dev);
	unsigned long flags;

	pm_runtime_get_sync(musb->controller);
	spin_lock_irqsave(&musb->lock, flags);
	musb_platform_disable(musb);
	musb_generic_disable(musb);
	spin_unlock_irqrestore(&musb->lock, flags);

	mu3d_writel(musb->mac_base, U3D_DEVICE_CONTROL, 0);
	musb_platform_exit(musb);

	pm_runtime_put(musb->controller);
	/* FIXME power down */
}
#endif

/*-------------------------------------------------------------------------*/

/*
 * The silicon either has hard-wired endpoint configurations, or else
 * "dynamic fifo" sizing.  The driver has support for both, though at this
 * writing only the dynamic sizing is very well tested.   Since we switched
 * away from compile-time hardware parameters, we can no longer rely on
 * dead code elimination to leave only the relevant one in the object file.
 *
 * We don't currently use dynamic fifo setup capability to do anything
 * more than selecting one of a bunch of predefined configurations.
 */

/* static ushort __initdata fifo_mode = 2; */


void ep0_setup(struct musb *musb, struct musb_hw_ep *hw_ep0, const struct musb_fifo_cfg *cfg)
{
	u32 ep0csr_val;

	mu3d_dbg(K_INFO, "ep0_setup maxpacket: %d\n", cfg->maxpacket);

	hw_ep0->fifoaddr_rx = 0;
	hw_ep0->fifoaddr_tx = 0;
	hw_ep0->is_shared_fifo = true;
	hw_ep0->fifo = musb->mac_base + MU3D_FIFO_OFFSET(0);

	/* for U2 */
	hw_ep0->max_packet_sz_tx = cfg->maxpacket;
	hw_ep0->max_packet_sz_rx = cfg->maxpacket;

	/* Defines the maximum amount of data that can be transferred through EP0 in a single operation. */
	/* os_writelmskumsk((void __iomem *)U3D_EP0CSR, hw_ep0->max_packet_sz_tx, EP0_MAXPKTSZ0, EP0_W1C_BITS); */
	ep0csr_val = mu3d_readl(musb->mac_base, U3D_EP0CSR);
	ep0csr_val &= ~EP0_MAXPKTSZ0;
	ep0csr_val |= (hw_ep0->max_packet_sz_tx & EP0_MAXPKTSZ0);
	ep0csr_val &= EP0_W1C_BITS;
	mu3d_writel(musb->mac_base, U3D_EP0CSR, ep0csr_val);

	/* Enable EP0 interrupt */
	mu3d_writel(musb->mac_base, U3D_EPIESR, EP0ISR);
}

/*
 * configure a fifo; for non-shared endpoints, this may be called
 * once for a tx fifo and once for an rx fifo.
 *
 * returns negative errno or offset for next fifo.
 */
static int
fifo_setup(struct musb *musb, struct musb_hw_ep *hw_ep, const struct musb_fifo_cfg *cfg, u16 offset)
{
	u16 maxpacket = cfg->maxpacket;
	/* u16   c_off = offset >> 3; */
	u16 ret_offset = 0;
	u32 maxpreg = 0;
	u8 mult = 0;

	/* calculate mult. added for ssusb. */
	if (maxpacket > 1024) {
		maxpreg = 1024;
		mult = (maxpacket / 1024) - 1;
	} else {
		maxpreg = maxpacket;
		/* set EP0 TX/RX slot to 3 by default */
		/* REVISIT-J: WHY? CHECK! */
		/* if (hw_ep->epnum == 1) */
		/*      mult = 3; */
	}

	/*REVISIT-J: WHY? CHECK! EP1 as BULK EP */
	/* EP0 reserved endpoint for control, bidirectional;
	 * EP1 reserved for bulk, two unidirection halves.
	 */
	/* if (hw_ep->epnum == 1) */
	/* musb->bulk_ep = hw_ep; */
	/* REVISIT error check:  be sure ep0 can both rx and tx ... */
	if ((cfg->style == FIFO_TX) || (cfg->style == FIFO_RXTX)) {
		hw_ep->max_packet_sz_tx = maxpreg;
		hw_ep->mult_tx = mult;

		hw_ep->fifoaddr_tx = musb->txfifoadd_offset;
		if (maxpacket == 1023)
			musb->txfifoadd_offset += (1024 * (hw_ep->mult_tx + 1));
		else
			musb->txfifoadd_offset += (maxpacket * (hw_ep->mult_tx + 1));
		ret_offset = musb->txfifoadd_offset;
	}

	if ((cfg->style == FIFO_RX) || (cfg->style == FIFO_RXTX)) {
		hw_ep->max_packet_sz_rx = maxpreg;
		hw_ep->mult_rx = mult;

		hw_ep->fifoaddr_rx = musb->rxfifoadd_offset;
		if (maxpacket == 1023)
			musb->rxfifoadd_offset += (1024 * (hw_ep->mult_rx + 1));
		else
			musb->rxfifoadd_offset += (maxpacket * (hw_ep->mult_rx + 1));
		ret_offset = musb->rxfifoadd_offset;
	}

	/* NOTE rx and tx endpoint irqs aren't managed separately,
	 * which happens to be ok
	 */
	musb->epmask |= (1 << hw_ep->epnum);

	return ret_offset;

}


struct musb_fifo_cfg ep0_cfg_u3 = {
	.style = FIFO_RXTX, .maxpacket = 512,
};

struct musb_fifo_cfg ep0_cfg_u2 = {
	.style = FIFO_RXTX, .maxpacket = 64,
};

static int ep_config_from_table(struct musb *musb)
{
	const struct musb_fifo_cfg *cfg = NULL;
	unsigned i, n = 0;
	int offset = 0;
	struct musb_hw_ep *hw_ep = musb->endpoints;

	if (musb->config->fifo_cfg) {
		cfg = musb->config->fifo_cfg;
		n = musb->config->fifo_cfg_size;
		mu3d_dbg(K_DEBUG, "%s: usb pre-cfg fifo_mode cfg=%p sz=%d\n", musb_driver_name,
			 cfg, n);
	} else {
		mu3d_dbg(K_ERR, "%s: SHOULD provide usb ep config table\n", musb_driver_name);
		return -EINVAL;
	}

#ifdef USB_GADGET_SUPERSPEED	/* SS */
	/* use SS EP0 as default; it may be changed later */
	mu3d_dbg(K_INFO, "%s ep_config_from_table ep0_cfg_u3\n", __func__);
	ep0_setup(musb, hw_ep, &ep0_cfg_u3);
#else				/* HS, FS */
	mu3d_dbg(K_INFO, "%s ep_config_from_table ep0_cfg_u2\n", __func__);
	ep0_setup(musb, hw_ep, &ep0_cfg_u2);
#endif
	/* assert(offset > 0) */

	/* NOTE:  for RTL versions >= 1.400 EPINFO and RAMINFO would
	 * be better than static musb->config->num_eps and DYN_FIFO_SIZE...
	 */

	for (i = 0; i < n; i++) {
		u8 epn = cfg->hw_ep_num;

		if (epn >= musb->config->num_eps) {
			mu3d_dbg(K_ERR, "%s: invalid ep %d\n", musb_driver_name, epn);
			return -EINVAL;
		}
		offset = fifo_setup(musb, hw_ep + epn, cfg++, offset);
		if (offset < 0) {
			mu3d_dbg(K_ERR, "%s: mem overrun, ep %d\n", musb_driver_name, epn);
			return -EINVAL;
		}
		epn++;
		musb->nr_endpoints = max(epn, musb->nr_endpoints);
	}

	mu3d_dbg(K_INFO, "%s: %d/%d max ep, %d/%d memory\n",
		 musb_driver_name,
		 n + 1, musb->config->num_eps * 2 - 1, offset, (1 << (musb->config->ram_bits + 2)));

	/* if (!musb->bulk_ep) { */
	/* pr_debug("%s: missing bulk\n", musb_driver_name); */
	/* return -EINVAL; */
	/* } */

	return 0;
}

/* Initialize MUSB (M)HDRC part of the USB hardware subsystem;
 * configure endpoints, or take their config from silicon
 */
static int musb_core_init(struct musb *musb)
{
	/* u8 reg; */
	/* char *type; */
	/* char aInfo[90], aRevision[32], aDate[12]; */
	void __iomem *mbase = musb->mac_base;
	u32 cap_epinfo;
	int status = 0;
	int i;

	mu3d_dbg(K_INFO, "%s() in (%#x)\n", __func__, U3D_SSUSB_HW_ID);

	musb->hwvers = mu3d_readl(musb->sif_base, U3D_SSUSB_HW_ID);

	mu3d_dbg(K_INFO, "%s: HDC version 0x%x\n", musb_driver_name, musb->hwvers);

	/* add for U3D */
	musb->txfifoadd_offset = U3D_FIFO_START_ADDRESS;
	musb->rxfifoadd_offset = U3D_FIFO_START_ADDRESS;

	mu3d_dbg(K_INFO, "%s EPnFIFOSZ Tx=%x, Rx=%x\n", __func__,
		 mu3d_readl(mbase, U3D_CAP_EPNTXFFSZ), mu3d_readl(mbase, U3D_CAP_EPNRXFFSZ));
	cap_epinfo = mu3d_readl(mbase, U3D_CAP_EPINFO);
	mu3d_dbg(K_INFO, "%s EPnNum Tx=%x, Rx=%d\n", __func__, cap_epinfo & 0x1F,
		 (cap_epinfo >> 8) & 0x1F);

	/* if (os_readl(U3D_CAP_EPNTXFFSZ) && os_readl(U3D_CAP_EPNRXFFSZ)) */
	/* musb->dyn_fifo = true; */
	/* else */
	/* musb->dyn_fifo = false; */

	/* discover endpoint configuration */
	musb->nr_endpoints = 1;
	musb->epmask = 1;

	/* status = ep_config_from_table(musb); */
	/* if (musb->dyn_fifo) */
	status = ep_config_from_table(musb);
	/* else */
	/* status = ep_config_from_hw(musb); */

	if (status < 0)
		return status;

	/* finish init, and print endpoint config */
	for (i = 0; i < musb->nr_endpoints; i++) {
		struct musb_hw_ep *hw_ep = musb->endpoints + i;

		hw_ep->fifo = mbase + MU3D_FIFO_OFFSET(i);

		/* change data structure for ssusb */
		if (i > 0) {
			hw_ep->addr_txcsr0 = MU3D_EP_TXCR0_OFFSET(i, mbase);
			hw_ep->addr_txcsr1 = MU3D_EP_TXCR1_OFFSET(i, mbase);
			hw_ep->addr_txcsr2 = MU3D_EP_TXCR2_OFFSET(i, mbase);
			hw_ep->addr_rxcsr0 = MU3D_EP_RXCR0_OFFSET(i, mbase);
			hw_ep->addr_rxcsr1 = MU3D_EP_RXCR1_OFFSET(i, mbase);
			hw_ep->addr_rxcsr2 = MU3D_EP_RXCR2_OFFSET(i, mbase);
			hw_ep->addr_rxcsr3 = MU3D_EP_RXCR3_OFFSET(i, mbase);
		}
		/* hw_ep->target_regs = musb_read_target_reg_base(i, mbase); */
		/* hw_ep->rx_reinit = 1; */
		/* hw_ep->tx_reinit = 1; */

		if (hw_ep->max_packet_sz_tx) {
			dev_dbg(musb->controller,
				"%s: hw_ep %d%s, %smax %d\n",
				musb_driver_name, i,
				hw_ep->is_shared_fifo ? "shared" : "tx",
				"", hw_ep->max_packet_sz_tx);
		}
		if (hw_ep->max_packet_sz_rx && !hw_ep->is_shared_fifo) {
			dev_dbg(musb->controller,
				"%s: hw_ep %d%s, %smax %d\n",
				musb_driver_name, i, "rx", "", hw_ep->max_packet_sz_rx);
		}
		if (!(hw_ep->max_packet_sz_tx || hw_ep->max_packet_sz_rx))
			dev_dbg(musb->controller, "hw_ep %d not configured\n", i);
	}

#ifdef USE_SSUSB_QMU
	/* Allocate GBD and BD */
	mu3d_hal_alloc_qmu_mem(musb);
	/* Iniital QMU */
	mu3d_hal_init_qmu(musb);

	musb_save_context(musb);
#endif

	return 0;
}

/*
 * handle all the irqs defined by the HDRC core. for now we expect:  other
 * irq sources (phy, dma, etc) will be handled first, musb->int_* values
 * will be assigned, and the irq will already have been acked.
 *
 * called in irq context with spinlock held, irqs blocked
 */
irqreturn_t musb_interrupt(struct musb *musb)
{
	irqreturn_t retval = IRQ_NONE;
	u32 devctl, power = 0;
#ifndef USE_SSUSB_QMU
	u32 reg = 0, ep_num = 0;
#endif

	devctl = mu3d_readl(musb->mac_base, U3D_DEVICE_CONTROL);
	power = mu3d_readl(musb->mac_base, U3D_POWER_MANAGEMENT);


	/* dev_dbg(musb->controller, "** IRQ %s usb%04x tx%04x rx%04x\n", */
	mu3d_dbg(K_DEBUG, "IRQ usb%04x tx%04x rx%04x\n",
		 /*(devctl & HOSTMODE) ? "host" : "peripheral", */
		 musb->int_usb, musb->int_tx, musb->int_rx);

	/* the core can interrupt us for multiple reasons; docs have
	 * a generic interrupt flowchart to follow
	 */
	if (musb->int_usb)
		retval |= musb_stage0_irq(musb, musb->int_usb, devctl, power);

	/* "stage 1" is handling endpoint irqs */

	/* handle endpoint 0 first */
	if (musb->int_tx & 1)
		retval |= musb_g_ep0_irq(musb);
#ifndef USE_SSUSB_QMU
	/* RX on endpoints 1-15 */
	reg = musb->int_rx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* musb_ep_select(musb->mregs, ep_num); */
			/* REVISIT just retval = ep->rx_irq(...) */
			retval = IRQ_HANDLED;
			musb_g_rx(musb, ep_num);
		}

		reg >>= 1;
		ep_num++;
	}

	/* TX on endpoints 1-15 */
	reg = musb->int_tx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* musb_ep_select(musb->mregs, ep_num); */
			/* REVISIT just retval |= ep->tx_irq(...) */
			retval = IRQ_HANDLED;
			musb_g_tx(musb, ep_num);
		}
		reg >>= 1;
		ep_num++;
	}
#endif

	return retval;
}

/* EXPORT_SYMBOL_GPL(musb_interrupt); */

/*-------------------------------------------------------------------------*/


static void musb_save_context(struct musb *musb)
{
	int i;

	for (i = 0; i < musb->config->num_eps; ++i) {
		mu3d_dbg(K_DEBUG, "%s EP%d\n", __func__, i);
#ifdef USE_SSUSB_QMU
		/* Save TXQ/RXQ starting address. Those would be reset to 0 after reset SSUSB IP. */
		musb->context.index_regs[i].txqmuaddr =
		    mu3d_readl(musb->mac_base, USB_QMU_TQSAR(i + 1));
		mu3d_dbg(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			 musb->context.index_regs[i].txqmuaddr);
		musb->context.index_regs[i].rxqmuaddr =
		    mu3d_readl(musb->mac_base, USB_QMU_RQSAR(i + 1));
		mu3d_dbg(K_DEBUG, "%s RQSAR[%d]=%x\n", __func__, i,
			 musb->context.index_regs[i].rxqmuaddr);
#endif
	}
}

static void musb_restore_context(struct musb *musb)
{
	int i;

	for (i = 0; i < musb->config->num_eps; ++i) {
#ifdef USE_SSUSB_QMU
		void __iomem *mbase = musb->mac_base;

		mu3d_writel(mbase, USB_QMU_TQSAR(i + 1), musb->context.index_regs[i].txqmuaddr);
		mu3d_writel(mbase, USB_QMU_RQSAR(i + 1), musb->context.index_regs[i].rxqmuaddr);
		mu3d_dbg(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			 mu3d_readl(mbase, USB_QMU_TQSAR(i + 1)));
		mu3d_dbg(K_DEBUG, "%s RQSAR[%d]=%x\n", __func__, i,
			 mu3d_readl(mbase, USB_QMU_RQSAR(i + 1)));
#endif
	}
}

static void musb_suspend_work(struct work_struct *data)
{
	struct musb *musb = container_of(data, struct musb, suspend_work);

	mu3d_dbg(K_INFO, "%s active_ep=%d, clk_on=%d\n", __func__, musb->active_ep,
		 musb->is_clk_on);

	if (musb->is_clk_on == 1  && !usb_cable_connected()) {
		/*
		* Note: musb_save_context() _MUST_ be called _BEFORE_ mtu3d_suspend_noirq().
		* Because when mtu3d_suspend_noirq() resets the SSUSB IP, All MAC regs can _NOT_ be read and be reset to
		* the default value. So save the MUST-SAVED reg in the context structure.
		*/
		musb_save_context(musb);

	/* Set SSUSB_IP_SW_RST to avoid power leakage */
	/* otg//mu3d_setmsk(musb->sif_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST); */
	/* it will disable xhci at the same time, maybe its not we want, pdw device only here */
	mu3d_hal_ssusb_dis(musb);
	ssusb_power_save(musb->ssusb);

#ifndef CONFIG_MTK_FPGA
	/* Let PHY enter savecurrent mode. And turn off CLK. */
	/* otg//u3phy->u3p_ops->usb_phy_savecurrent(u3phy, musb->is_clk_on); */
	musb->is_clk_on = 0;
#endif
	}
}

/* Only used to provide driver mode change events */
static void musb_otg_event_work(struct work_struct *data)
{
	struct musb *musb = container_of(data, struct musb, otg_event_work);
	static int old_state;

	mu3d_dbg(K_INFO, "%s [%d]=[%d]\n", __func__, musb->xceiv->state, old_state);

	if (musb->xceiv->state != old_state) {
		old_state = musb->xceiv->state;
		sysfs_notify(&musb->controller->kobj, NULL, "mode");
	}

}


/* --------------------------------------------------------------------------
 * Init support
 */
#if 0
static void __iomem *get_regs_base(struct platform_device *pdev, int index)
{
	struct resource *res;
	void __iomem *regs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	if (NULL == res) {
		mu3d_dbg(K_ERR, "error to get rsrc %d\n", index);
		return NULL;
	}

	regs = ioremap_nocache(res->start, resource_size(res));
	if (NULL == regs) {
		mu3d_dbg(K_ERR, "error mapping memory for %d\n", index);
		return NULL;
	}
	mu3d_dbg(K_INFO, "index:%d - iomap:0x%p, res:%#lx, len:%#lx\n", index, regs,
		 (unsigned long)res->start, (unsigned long)resource_size(res));
	return regs;
}
#endif

static struct musb *allocate_instance(struct device *dev, struct musb_hdrc_config *config)
{
	struct musb *musb;
	struct musb_hw_ep *ep;
	int epnum;

	musb = kzalloc(sizeof(struct musb), GFP_KERNEL);
	if (NULL == musb)
		return NULL;

	/* INIT_LIST_HEAD(&musb->control); */
	/* INIT_LIST_HEAD(&musb->in_bulk); */
	/* INIT_LIST_HEAD(&musb->out_bulk); */
	spin_lock_init(&musb->lock);

	/* dev_set_drvdata(dev, musb); rm musb layer */
	musb->config = config;
	BUG_ON(musb->config->num_eps > MUSB_C_NUM_EPS);
	for (epnum = 0, ep = musb->endpoints; epnum < musb->config->num_eps; epnum++, ep++) {
		ep->musb = musb;
		ep->epnum = epnum;
	}

	musb->controller = dev;

	/* added for ssusb: */
	/* musb->xceiv = kzalloc(sizeof(struct otg_transceiver), GFP_KERNEL); */
	/* memset(musb->xceiv, 0, sizeof(struct otg_transceiver)); */
	/* musb->xceiv->state = OTG_STATE_B_IDLE; //initial its value */

	return musb;
}

static void musb_free(struct musb *musb)
{
	/* this has multiple entry modes. it handles fault cleanup after
	 * probe(), where things may be partially set up, as well as rmmod
	 * cleanup after everything's been de-activated.
	 */

	ssusb_sysfs_exit(musb);

	musb_gadget_cleanup(musb);

	if (musb->irq >= 0) {
		if (musb->irq_wake)
			disable_irq_wake(musb->irq);
		free_irq(musb->irq, musb);
	}

	cancel_work_sync(&musb->otg_event_work);
	cancel_delayed_work_sync(&musb->connection_work);
	/* cancel_delayed_work_sync(&musb->check_ltssm_work); */
	cancel_work_sync(&musb->suspend_work);

#ifdef USE_SSUSB_QMU
	tasklet_kill(&musb->qmu_done);
	mu3d_hal_free_qmu_mem(musb);
#endif
	wake_lock_destroy(&musb->usb_wakelock);

	/* added for ssusb: */
	kfree(musb->xceiv);	/* free the instance allocated in allocate_instance */
	musb->xceiv = NULL;

	kfree(musb);
}

/*
 * Perform generic per-controller initialization.
 *
 * @pDevice: the controller (already clocked, etc)
 * @irq: irq number
 * @mregs: virtual address of controller registers,
 *	not yet corrected for platform-specific offsets
 */
static int musb_init_controller(struct musb *musb, struct device *dev, int irq)
{
	struct musb_hdrc_platform_data *plat = dev->platform_data;
	int status;

	/* The driver might handle more features than the board; OK.
	 * Fail when the board needs a feature that's not enabled.
	 */

	mu3d_dbg(K_INFO, "[MU3D]%s\n", __func__);


	musb->board_mode = plat->drv_mode;
	/* musb->board_mode = MUSB_PERIPHERAL;  for test , rm later */
	/* musb->board_set_power = plat->set_power; */
	/* musb->min_power = plat->min_power; */
	musb->ops = plat->platform_ops;
	musb->usb_mode = 1;

	_mu3d_musb = musb;

	wake_lock_init(&musb->usb_wakelock, WAKE_LOCK_SUSPEND, "USB.lock");

	musb->wq = create_singlethread_workqueue("usb_work");
	if (!musb->wq) {
		status = -ENOMEM;
		goto fail0;
	}

	INIT_DELAYED_WORK(&musb->connection_work, connection_work);
	mu3d_dbg(K_INFO, "[MU3D]%s %d\n", __func__, __LINE__);

	/* The musb_platform_init() call:
	 *   - adjusts musb->mregs and musb->isr if needed,
	 *   - may initialize an integrated tranceiver
	 *   - initializes musb->xceiv, usually by otg_get_transceiver()
	 *   - stops powering VBUS
	 *
	 * There are various transceiver configurations.  Blackfin,
	 * DaVinci, TUSB60x0, and others integrate them.  OMAP3 uses
	 * external/discrete ones in various flavors (twl4030 family,
	 * isp1504, non-OTG, etc) mostly hooking up through ULPI.
	 */
	/*move to musb_init.c */
	/*musb->isr = generic_interrupt; */
	status = musb_platform_init(musb);

	if (status < 0)
		goto fail1;

	if (!musb->isr) {
		status = -ENODEV;
		goto fail3;
	}
	pm_runtime_get_sync(musb->controller);

	/* ideally this would be abstracted in platform setup */
	/* if (!is_dma_capable() || !musb->dma_controller) */
	/* dev->dma_mask = NULL; */

	/* be sure interrupts are disabled before connecting ISR */
	musb_platform_disable(musb);
	musb_generic_disable(musb);

	/* setup musb parts of the core (especially endpoints) */
	status = musb_core_init(musb);
	if (status < 0)
		goto fail3;

	/* REVISIT-J: Do _NOT_ support OTG functionality */
	/* setup_timer(&musb->otg_timer, musb_otg_timer_func, (unsigned long) musb); */

	INIT_WORK(&musb->otg_event_work, musb_otg_event_work);
	INIT_WORK(&musb->suspend_work, musb_suspend_work);

#ifdef USE_SSUSB_QMU
	tasklet_init(&musb->qmu_done, qmu_done_tasklet, (unsigned long)musb);
#endif

	/* attach to the IRQ */
	mu3d_dbg(K_INFO, "%s -> %d irq%d, isr:%p, %s, musb:%p\n", __func__, __LINE__,
		 irq, musb->isr, dev_name(dev), musb);
	if (request_irq(irq, musb->isr, IRQF_TRIGGER_LOW, dev_name(dev), musb)) {
		dev_err(dev, "request_irq %d failed!\n", irq);
		status = -ENODEV;
		goto fail3;
	}
	musb->irq = irq;
	/* FIXME this handles wakeup irqs wrong */
	if (enable_irq_wake(irq) == 0) {
		musb->irq_wake = 1;
		device_init_wakeup(dev, 1);
	} else {
		musb->irq_wake = 0;
	}

	/* MUSB_DEV_MODE(musb); */
	musb->xceiv->otg->default_a = 0;
	musb->xceiv->state = OTG_STATE_B_IDLE;

	status = musb_gadget_setup(musb);

	if (status < 0)
		goto fail4;

	status = ssusb_sysfs_init(musb);
	if (status)
		goto fail5;

	pm_runtime_put(musb->controller);

	dev_info(dev, "MU3D using %s, IRQ %d\n", (is_dma_capable()) ? "QMU" : "PIO", musb->irq);

	return 0;


fail5:
	musb_gadget_cleanup(musb);

fail4:
	if (musb->irq_wake)
		device_init_wakeup(dev, 0);
	free_irq(musb->irq, musb);
fail3:
	musb_platform_exit(musb);

fail1:
	destroy_workqueue(musb->wq);
fail0:
	_mu3d_musb = NULL;
	mu3d_dbg(K_ERR, "%s() failed with status %d\n", __func__, status);
	return status;

}

/*-------------------------------------------------------------------------*/

int ssusb_gadget_init(struct ssusb_mtk *ssusb)
{
	struct musb_hdrc_platform_data *pdata = ssusb->pdata;
	struct device *dev = ssusb->dev;
	struct musb *musb = NULL;
	int retval = -ENOMEM;


	musb = allocate_instance(dev, pdata->config);
	if (NULL == musb) {
		mu3d_dbg(K_ERR, "fail to alloc musb\n");
		retval = -ENOMEM;
		goto musb_err;
	}

	musb->mac_base = ssusb->mac_base;
	musb->sif_base = ssusb->sif_base;
	/* musb->sif2_base = ssusb->sif2_base; */
	musb->start_mu3d = ssusb->start_mu3d;
	ssusb->mu3di = musb;
	musb->ssusb = ssusb;

	mu3d_dbg(K_INFO, "mac_base=0x%p, sif_base=0x%p\n", musb->mac_base, musb->sif_base);

	retval = musb_init_controller(musb, dev, ssusb->mu3d_irq);
	if (retval < 0)
		goto mi_err;

	mu3d_dbg(K_INFO, " %s() success...\n", __func__);

	return 0;

mi_err:
	kfree(musb);
	ssusb->mu3di = NULL;

musb_err:
	mu3d_dbg(K_ERR, " %s() fail...\n", __func__);
	return retval;
}

void ssusb_gadget_exit(struct ssusb_mtk *ssusb)
{
	struct musb *musb = ssusb->mu3di;

	/* this gets called on rmmod.
	 *  - Host mode: host may still be active
	 *  - Peripheral mode: peripheral is deactivated (or never-activated)
	 *  - OTG mode: both roles are deactivated (or never-activated)
	 */
	pm_runtime_get_sync(musb->controller);
	mu3d_writel(musb->mac_base, U3D_DEVICE_CONTROL, 0);
	musb_platform_exit(musb);
	pm_runtime_put(musb->controller);

	musb_free(musb);
	_mu3d_musb = NULL;
	device_init_wakeup(ssusb->dev, 0);

}
