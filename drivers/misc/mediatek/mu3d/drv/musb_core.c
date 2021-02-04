/*
 * MUSB OTG driver core code
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
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
#ifdef CONFIG_USB_C_SWITCH
#include <typec.h>
#endif
#ifdef CONFIG_USBIF_COMPLIANCE
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#endif

#include <mt-plat/mtk_chip.h>
#include "musb_core.h"
#include "mu3d_hal_osal.h"
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "ssusb_qmu.h"

#include <linux/phy/mediatek/mtk_usb_phy.h>

#ifdef CONFIG_MTK_UART_USB_SWITCH
#define AP_UART0_COMPATIBLE_NAME "mediatek,gpio"
#endif

#define AP_PLL_CON0_COMPATIBLE_NAME "mediatek,apmixed"

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#define TA_WAIT_BCON(m) max_t(int, (m)->a_wait_bcon, OTG_TIME_A_WAIT_BCON)


#define DRIVER_AUTHOR "Mentor Graphics, Texas Instruments, Nokia"
#define DRIVER_DESC "Inventra Dual-Role USB Controller Driver"

#define MUSB_VERSION "6.0"

#define DRIVER_INFO DRIVER_DESC ", v" MUSB_VERSION

const char musb_driver_name[] = MUSB_DRIVER_NAME;

struct musb *_mu3d_musb;


u32 debug_level = K_WARNIN;
u32 fake_CDP;

module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug Print Log Lvl");
module_param(fake_CDP, int, 0644);

#ifdef EP_PROFILING
u32 is_prof = 1;

module_param(is_prof, int, 0644);
MODULE_PARM_DESC(is_prof, "profiling each EP");
#endif

MODULE_DESCRIPTION(DRIVER_INFO);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" MUSB_DRIVER_NAME);

#define U3D_FIFO_START_ADDRESS 0

#ifdef SUPPORT_U3
/*
 * USB Speed Mode
 * 0: High Speed
 * 1: Super Speed
 */
unsigned int musb_speed;
static int set_musb_speed(const char *val, const struct kernel_param *kp)
{
	int rv;

	/* update module parameter */
	rv = param_set_int(val, kp);
	if (rv)
		return rv;

	musb_hal_speed = musb_speed;

	pr_warn("musb_speed:%d, musb_hal_speed:%d\n", musb_speed, musb_hal_speed);

	return 0;
}
static struct kernel_param_ops musb_speed_param_ops = {
	.set = set_musb_speed,
	.get = param_get_int,
};
module_param_cb(speed, &musb_speed_param_ops, &musb_speed, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "USB speed configuration. default = 1, spuper speed.");
#endif

void __iomem *u3_base;
void __iomem *u3_ippc_base;
void __iomem *u3_sif2_base;
void __iomem *ap_uart0_base;
void __iomem *ap_pll_con0;

#ifdef CONFIG_FPGA_EARLY_PORTING
void __iomem *i2c_base;
u32 i2c_physical_base;
#endif

/*-------------------------------------------------------------------------*/

static inline struct musb *dev_to_musb(struct device *dev)
{
	return dev_get_drvdata(dev);
}

/*-------------------------------------------------------------------------*/

#if 0
/* #ifndef CONFIG_BLACKFIN */
static int musb_ulpi_read(struct otg_transceiver *otg, u32 offset)
{
	void __iomem *addr = otg->io_priv;
	int i = 0;
	u8 r;
	u8 power;

	/* Make sure the transceiver is not in low power mode */
	power = musb_readb(addr, MUSB_POWER);
	power &= ~MUSB_POWER_SUSPENDM;
	musb_writeb(addr, MUSB_POWER, power);

	/* REVISIT: musbhdrc_ulpi_an.pdf recommends setting the
	 * ULPICarKitControlDisableUTMI after clearing POWER_SUSPENDM.
	 */

	musb_writeb(addr, MUSB_ULPI_REG_ADDR, (u8) offset);
	musb_writeb(addr, MUSB_ULPI_REG_CONTROL, MUSB_ULPI_REG_REQ | MUSB_ULPI_RDN_WR);

	while (!(musb_readb(addr, MUSB_ULPI_REG_CONTROL)
		 & MUSB_ULPI_REG_CMPLT)) {
		i++;
		if (i == 10000)
			return -ETIMEDOUT;

	}
	r = musb_readb(addr, MUSB_ULPI_REG_CONTROL);
	r &= ~MUSB_ULPI_REG_CMPLT;
	musb_writeb(addr, MUSB_ULPI_REG_CONTROL, r);

	return musb_readb(addr, MUSB_ULPI_REG_DATA);
}

static int musb_ulpi_write(struct otg_transceiver *otg, u32 offset, u32 data)
{
	void __iomem *addr = otg->io_priv;
	int i = 0;
	u8 r = 0;
	u8 power;

	/* Make sure the transceiver is not in low power mode */
	power = musb_readb(addr, MUSB_POWER);
	power &= ~MUSB_POWER_SUSPENDM;
	musb_writeb(addr, MUSB_POWER, power);

	musb_writeb(addr, MUSB_ULPI_REG_ADDR, (u8) offset);
	musb_writeb(addr, MUSB_ULPI_REG_DATA, (u8) data);
	musb_writeb(addr, MUSB_ULPI_REG_CONTROL, MUSB_ULPI_REG_REQ);

	while (!(musb_readb(addr, MUSB_ULPI_REG_CONTROL)
		 & MUSB_ULPI_REG_CMPLT)) {
		i++;
		if (i == 10000)
			return -ETIMEDOUT;
	}

	r = musb_readb(addr, MUSB_ULPI_REG_CONTROL);
	r &= ~MUSB_ULPI_REG_CMPLT;
	musb_writeb(addr, MUSB_ULPI_REG_CONTROL, r);

	return 0;
}
#else
#define musb_ulpi_read		NULL
#define musb_ulpi_write		NULL
#endif

/*
 * Load an endpoint's FIFO
 */
void musb_write_fifo(struct musb_hw_ep *hw_ep, u16 len, const u8 *src)
{
	unsigned int residue;
	unsigned int temp;

	/* QMU GPD address --> CPU DMA address */
	void __iomem *fifo = (void __iomem *)(uintptr_t) USB_FIFO(hw_ep->epnum);

	os_printk(K_DEBUG, "%s epnum=%d, len=%d, buf=%p\n", __func__, hw_ep->epnum, len, src);

	residue = len;

	while (residue > 0) {

		if (residue == 1) {
			temp = ((*src) & 0xFF);
			/* os_writeb(fifo, temp); */
			writeb(temp, fifo);
			src += 1;
			residue -= 1;
		} else if (residue == 2) {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00);
			/* os_writew(fifo, temp); */
			writew(temp, fifo);
			src += 2;
			residue -= 2;
		} else if (residue == 3) {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00);
			/* os_writew(fifo, temp); */
			writew(temp, fifo);
			src += 2;

			temp = ((*src) & 0xFF);
			/* os_writeb(fifo, temp); */
			writeb(temp, fifo);
			src += 1;
			residue -= 3;
		} else {
			temp = ((*src) & 0xFF) + (((*(src + 1)) << 8) & 0xFF00) +
			    (((*(src + 2)) << 16) & 0xFF0000) + (((*(src + 3)) << 24) & 0xFF000000);
			/* os_writel(fifo, temp); */
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

	/* QMU GPD address --> CPU DMA address */
	void __iomem *fifo = (void __iomem *)(uintptr_t) USB_FIFO(hw_ep->epnum);

	os_printk(K_DEBUG, "%s %cX ep%d fifo %p count %d buf %p\n",
		  __func__, 'R', hw_ep->epnum, fifo, len, dst);

	residue = len;

	while (residue > 0) {

		temp = os_readl(fifo);

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
	mu3d_hal_write_fifo(0, sizeof(musb_test_packet), (u8 *) musb_test_packet, maxp);
}

/*-------------------------------------------------------------------------*/

/*
 * Handles OTG hnp timeouts, such as b_ase0_brst
 */
void musb_otg_timer_func(unsigned long data)
{
	struct musb *musb = (struct musb *)data;
	unsigned long flags;

	spin_lock_irqsave(&musb->lock, flags);
	switch (musb->xceiv->otg->state) {
	case OTG_STATE_B_WAIT_ACON:
		dev_dbg(musb->controller, "HNP: b_wait_acon timeout; back to b_peripheral\n");
		musb_g_disconnect(musb);
		musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
		musb->is_active = 0;
		break;
	case OTG_STATE_A_SUSPEND:
	case OTG_STATE_A_WAIT_BCON:
		dev_dbg(musb->controller, "HNP: %s timeout\n",
			usb_otg_state_string(musb->xceiv->otg->state));
		musb_platform_set_vbus(musb, 0);
		musb->xceiv->otg->state = OTG_STATE_A_WAIT_VFALL;
		break;
	default:
		dev_dbg(musb->controller, "HNP: Unhandled mode %s\n",
			usb_otg_state_string(musb->xceiv->otg->state));
	}
	musb->ignore_disconnect = 0;
	spin_unlock_irqrestore(&musb->lock, flags);
}

/*
 * Stops the HNP transition. Caller must take care of locking.
 */
void musb_hnp_stop(struct musb *musb)
{
	struct usb_hcd *hcd = musb_to_hcd(musb);
	u32 reg;

	dev_dbg(musb->controller, "HNP: stop from %s\n", usb_otg_state_string(musb->xceiv->otg->state));

	switch (musb->xceiv->otg->state) {
	case OTG_STATE_A_PERIPHERAL:
		musb_g_disconnect(musb);
		dev_dbg(musb->controller, "HNP: back to %s\n",
			usb_otg_state_string(musb->xceiv->otg->state));
		break;
	case OTG_STATE_B_HOST:
		dev_dbg(musb->controller, "HNP: Disabling HR\n");
		hcd->self.is_b_host = 0;
		musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
		MUSB_DEV_MODE(musb);
		/* reg = musb_readb(mbase, MUSB_POWER); */
		reg = os_readl(U3D_POWER_MANAGEMENT);
		reg |= SUSPENDM_ENABLE;
		os_writel(U3D_POWER_MANAGEMENT, reg);
		/* REVISIT: Start SESSION_REQUEST here? */
		break;
	default:
		dev_dbg(musb->controller, "HNP: Stopping in unknown state %s\n",
			usb_otg_state_string(musb->xceiv->otg->state));
	}

	/*
	 * When returning to A state after HNP, avoid hub_port_rebounce(),
	 * which cause occasional OPT A "Did not receive reset after connect"
	 * errors.
	 */
	musb->port1_status &= ~(USB_PORT_STAT_C_CONNECTION << 16);
}

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

static irqreturn_t musb_stage0_irq(struct musb *musb, u32 int_usb, u8 devctl, u8 power)
{
	struct usb_otg *otg = musb->xceiv->otg;
	irqreturn_t handled = IRQ_NONE;

	os_printk(K_DEBUG, "<== Power=%02x, DevCtl=%02x, int_usb=0x%x\n", power, devctl, int_usb);

	/* in host mode, the peripheral may issue remote wakeup.
	 * in peripheral mode, the host may resume the link.
	 * spurious RESUME irqs happen too, paired with SUSPEND.
	 */
	if (int_usb & RESUME_INTR) {
		handled = IRQ_HANDLED;
		dev_notice(musb->controller, "RESUME (%s)\n",
			   usb_otg_state_string(musb->xceiv->otg->state));

		/* We implement device mode only. */
		switch (musb->xceiv->otg->state) {
		case OTG_STATE_A_SUSPEND:
			/* possibly DISCONNECT is upcoming */
			musb->xceiv->otg->state = OTG_STATE_A_HOST;
			usb_hcd_resume_root_hub(musb_to_hcd(musb));
			break;
		case OTG_STATE_B_WAIT_ACON:
		case OTG_STATE_B_PERIPHERAL:
			/* disconnect while suspended?  we may
			 * not get a disconnect irq...
			 */
			if ((devctl & USB_DEVCTL_VBUSVALID)
			    != (3 << USB_DEVCTL_VBUS_OFFSET)
			    ) {
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
			dev_warn(musb->controller, "bogus %s RESUME (%s)\n",
				"peripheral", usb_otg_state_string(musb->xceiv->otg->state));
		}
	}

	/* see manual for the order of the tests */
	if (int_usb & SESSION_REQ_INTR) {
		if ((devctl & USB_DEVCTL_VBUSMASK) == USB_DEVCTL_VBUSVALID
		    && (devctl & USB_DEVCTL_BDEVICE)) {
			dev_dbg(musb->controller, "SessReq while on B state\n");
			return IRQ_HANDLED;
		}

		dev_notice(musb->controller, "SESSION_REQUEST (%s)\n",
			   usb_otg_state_string(musb->xceiv->otg->state));

		/* IRQ arrives from ID pin sense or (later, if VBUS power
		 * is removed) SRP.  responses are time critical:
		 *  - turn on VBUS (with silicon-specific mechanism)
		 *  - go through A_WAIT_VRISE
		 *  - ... to A_WAIT_BCON.
		 * a_wait_vrise_tmout triggers VBUS_ERROR transitions
		 */
		/* os_writel(mregs + MAC_DEVICE_CONTROL, devctl & USB_DEVCTL_SESSION); */
		musb->ep0_stage = MUSB_EP0_START;
		musb->xceiv->otg->state = OTG_STATE_A_IDLE;
		MUSB_HST_MODE(musb);
		musb_platform_set_vbus(musb, 1);

		handled = IRQ_HANDLED;
	}

	if (int_usb & VBUSERR_INTR) {
		int ignore = 0;

		/* During connection as an A-Device, we may see a short
		 * current spikes causing voltage drop, because of cable
		 * and peripheral capacitance combined with vbus draw.
		 * (So: less common with truly self-powered devices, where
		 * vbus doesn't act like a power supply.)
		 *
		 * Such spikes are short; usually less than ~500 usec, max
		 * of ~2 msec.  That is, they're not sustained overcurrent
		 * errors, though they're reported using VBUSERROR irqs.
		 *
		 * Workarounds:  (a) hardware: use self powered devices.
		 * (b) software:  ignore non-repeated VBUS errors.
		 *
		 * REVISIT:  do delays from lots of DEBUG_KERNEL checks
		 * make trouble here, keeping VBUS < 4.4V ?
		 */
		switch (musb->xceiv->otg->state) {
		case OTG_STATE_A_HOST:
			/* recovery is dicey once we've gotten past the
			 * initial stages of enumeration, but if VBUS
			 * stayed ok at the other end of the link, and
			 * another reset is due (at least for high speed,
			 * to redo the chirp etc), it might work OK...
			 */
		case OTG_STATE_A_WAIT_BCON:
		case OTG_STATE_A_WAIT_VRISE:
			if (musb->vbuserr_retry) {
				musb->vbuserr_retry--;
				ignore = 1;
				devctl |= USB_DEVCTL_SESSION;
				/* os_writel(mregs + MAC_DEVICE_CONTROL, devctl & USB_DEVCTL_SESSION); */
			} else {
				musb->port1_status |=
				    USB_PORT_STAT_OVERCURRENT | (USB_PORT_STAT_C_OVERCURRENT << 16);
			}
			break;
		default:
			break;
		}

		dev_notice(musb->controller, "VBUS_ERROR in %s (%02x, %s), retry #%d, port1 %08x\n",
			   usb_otg_state_string(musb->xceiv->otg->state), devctl, ({
				char *s;

				switch (devctl &
				USB_DEVCTL_VBUSMASK) {
				case 0 << USB_DEVCTL_VBUS_OFFSET:
				s = "<SessEnd"; break; case 1 << USB_DEVCTL_VBUS_OFFSET:
				s = "<AValid"; break; case 2 << USB_DEVCTL_VBUS_OFFSET:
				s = "<VBusValid";
				break;
				/* case 3 << MUSB_DEVCTL_VBUS_SHIFT: */
				default:
				s = "VALID"; break; };
				s; }
			   ), VBUSERR_RETRY_COUNT - musb->vbuserr_retry, musb->port1_status);

		/* go through A_WAIT_VFALL then start a new session */
		if (!ignore)
			musb_platform_set_vbus(musb, 0);
		handled = IRQ_HANDLED;
	}

	if (int_usb & SUSPEND_INTR) {
		dev_notice(musb->controller, "SUSPEND (%s) devctl %02x power %02x\n",
			   usb_otg_state_string(musb->xceiv->otg->state), devctl, power);
		handled = IRQ_HANDLED;

		switch (musb->xceiv->otg->state) {
		case OTG_STATE_A_PERIPHERAL:
			/* We also come here if the cable is removed, since
			 * this silicon doesn't report ID-no-longer-grounded.
			 *
			 * We depend on T(a_wait_bcon) to shut us down, and
			 * hope users don't do anything dicey during this
			 * undesired detour through A_WAIT_BCON.
			 */
			musb_hnp_stop(musb);
			usb_hcd_resume_root_hub(musb_to_hcd(musb));
			/* musb_root_disconnect(musb); //I don't port virthub now. */
			musb_platform_try_idle(musb, jiffies
					       + msecs_to_jiffies(musb->a_wait_bcon
								  ? : OTG_TIME_A_WAIT_BCON));

			break;
		case OTG_STATE_B_IDLE:
			if (!musb->is_active)
				break;
		case OTG_STATE_B_PERIPHERAL:
			musb_g_suspend(musb);
			musb->is_active = is_otg_enabled(musb)
			    && otg->gadget->b_hnp_enable;
			if (musb->is_active) {
				musb->xceiv->otg->state = OTG_STATE_B_WAIT_ACON;
				dev_dbg(musb->controller, "HNP: Setting timer for b_ase0_brst\n");
				mod_timer(&musb->otg_timer, jiffies
					  + msecs_to_jiffies(OTG_TIME_B_ASE0_BRST));
			}
			break;
		case OTG_STATE_A_WAIT_BCON:
			if (musb->a_wait_bcon != 0)
				musb_platform_try_idle(musb, jiffies
						       + msecs_to_jiffies(musb->a_wait_bcon));
			break;
		case OTG_STATE_A_HOST:
			musb->xceiv->otg->state = OTG_STATE_A_SUSPEND;
			musb->is_active = is_otg_enabled(musb)
			    && otg->host->b_hnp_enable;
			break;
		case OTG_STATE_B_HOST:
			/* Transition to B_PERIPHERAL, see 6.8.2.6 p 44 */
			dev_dbg(musb->controller, "REVISIT: SUSPEND as B_HOST\n");
			break;
		default:
			/* "should not happen" */
			musb->is_active = 0;
			break;
		}
	}

	if (int_usb & CONN_INTR) {
		struct usb_hcd *hcd = musb_to_hcd(musb);
		u32 int_en = 0;

		handled = IRQ_HANDLED;
		musb->is_active = 1;

		musb->ep0_stage = MUSB_EP0_START;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_START\n");

		/* flush endpoints when transitioning from Device Mode */
		if (is_peripheral_active(musb)) {
			/* REVISIT HNP; */
			/* just force disconnect */
		}
		/* musb_writew(musb->mregs, MUSB_INTRTXE, musb->epmask); */
		/* musb_writew(musb->mregs, MUSB_INTRRXE, musb->epmask & 0xfffe); */
#ifdef USE_SSUSB_QMU
		/*Only Enable EP0 Tx interrupt */
		os_writel(U3D_EPIESR, os_readl(U3D_EPIESR) | EP0ISR);
#else
		/*Enable EP0 Tx and EPn Tx/Rx interrupt */
		os_printk(K_DEBUG, "Enable EP0 & EPn interrupt =%x\n",
			  musb->epmask | ((musb->epmask << 16) & EPRISR));
		os_writel(U3D_EPIESR, musb->epmask | ((musb->epmask << 16) & EPRISR));
#endif
		int_en =
		    SUSPEND_INTR_EN | RESUME_INTR_EN | RESET_INTR_EN | CONN_INTR_EN |
		    DISCONN_INTR_EN;

		os_writel(U3D_COMMON_USB_INTR_ENABLE, int_en);

		/* musb_writeb(musb->mregs, MUSB_INTRUSBE, 0xf7); */

		musb->port1_status &= ~(USB_PORT_STAT_LOW_SPEED
					| USB_PORT_STAT_HIGH_SPEED | USB_PORT_STAT_ENABLE);
		musb->port1_status |= USB_PORT_STAT_CONNECTION | (USB_PORT_STAT_C_CONNECTION << 16);

		/* high vs full speed is just a guess until after reset */
		if (devctl & USB_DEVCTL_LS_DEV)
			musb->port1_status |= USB_PORT_STAT_LOW_SPEED;

		/* indicate new connection to OTG machine */
		switch (musb->xceiv->otg->state) {
		case OTG_STATE_B_PERIPHERAL:
			if (int_usb & SUSPEND_INTR) {
				dev_dbg(musb->controller, "HNP: SUSPEND+CONNECT, now b_host\n");
				int_usb &= ~SUSPEND_INTR;
				goto b_host;
			} else
				dev_dbg(musb->controller, "CONNECT as b_peripheral???\n");
			break;
		case OTG_STATE_B_WAIT_ACON:
			dev_dbg(musb->controller, "HNP: CONNECT, now b_host\n");
b_host:
			musb->xceiv->otg->state = OTG_STATE_B_HOST;
			hcd->self.is_b_host = 1;
			musb->ignore_disconnect = 0;
			del_timer(&musb->otg_timer);
			break;
		default:
			if ((devctl & USB_DEVCTL_VBUSVALID)
			    == (3 << USB_DEVCTL_VBUS_OFFSET)) {
				musb->xceiv->otg->state = OTG_STATE_A_HOST;
				hcd->self.is_b_host = 0;
			}
			break;
		}

		/* poke the root hub */
		MUSB_HST_MODE(musb);
		if (hcd->status_urb)
			usb_hcd_poll_rh_status(hcd);
		else
			usb_hcd_resume_root_hub(hcd);

		dev_notice(musb->controller, "CONNECT (%s) devctl %02x\n",
			   usb_otg_state_string(musb->xceiv->otg->state), devctl);
	}

	if ((int_usb & DISCONN_INTR) && !musb->ignore_disconnect) {
		dev_notice(musb->controller, "DISCONNECT (%s) as %s, devctl %02x\n",
			   usb_otg_state_string(musb->xceiv->otg->state), MUSB_MODE(musb), devctl);
		handled = IRQ_HANDLED;

		switch (musb->xceiv->otg->state) {
		case OTG_STATE_A_HOST:
		case OTG_STATE_A_SUSPEND:
			usb_hcd_resume_root_hub(musb_to_hcd(musb));
/* musb_root_disconnect(musb); */
			if (musb->a_wait_bcon != 0 && is_otg_enabled(musb))
				musb_platform_try_idle(musb, jiffies
						       + msecs_to_jiffies(musb->a_wait_bcon));
			break;
		case OTG_STATE_B_HOST:
			/* REVISIT this behaves for "real disconnect"
			 * cases; make sure the other transitions from
			 * from B_HOST act right too.  The B_HOST code
			 * in hnp_stop() is currently not used...
			 */
			/* musb_root_disconnect(musb); //I don't port virthub now. */
			musb_to_hcd(musb)->self.is_b_host = 0;
			musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
			MUSB_DEV_MODE(musb);
			musb_g_disconnect(musb);
			break;
		case OTG_STATE_A_PERIPHERAL:
			musb_hnp_stop(musb);
			/* musb_root_disconnect(musb); //I don't port virthub now. */
			/* FALLTHROUGH */
		case OTG_STATE_B_WAIT_ACON:
			/* FALLTHROUGH */
		case OTG_STATE_B_PERIPHERAL:
		case OTG_STATE_B_IDLE:
			musb_g_disconnect(musb);
			break;
		default:
			dev_warn(musb->controller, "unhandled DISCONNECT transition (%s)\n",
				usb_otg_state_string(musb->xceiv->otg->state));
			break;
		}
	}

	/* mentor saves a bit: bus reset and babble share the same irq.
	 * only host sees babble; only peripheral sees bus reset.
	 */
	if (int_usb & RESET_INTR) {
		handled = IRQ_HANDLED;

		mu3d_hal_pdn_ip_port(1, 0, 1, 1);

		if (1) {	/* device mode */
			/* dev_notice(musb->controller, "BUS RESET as %s\n",
			 * usb_otg_state_string(musb->xceiv->otg->state));
			 */
			os_printk(K_DEBUG, "BUS RESET\n");
			switch (musb->xceiv->otg->state) {
			case OTG_STATE_A_SUSPEND:
				/* We need to ignore disconnect on suspend
				 * otherwise tusb 2.0 won't reconnect after a
				 * power cycle, which breaks otg compliance.
				 */
				musb->ignore_disconnect = 1;
				musb_g_reset(musb);
				/* FALLTHROUGH */
			case OTG_STATE_A_WAIT_BCON:	/* OPT TD.4.7-900ms */
				/* never use invalid T(a_wait_bcon) */
				dev_dbg(musb->controller, "HNP: in %s, %d msec timeout\n",
					usb_otg_state_string(musb->xceiv->otg->state),
					TA_WAIT_BCON(musb));
				mod_timer(&musb->otg_timer, jiffies
					  + msecs_to_jiffies(TA_WAIT_BCON(musb)));
				break;
			case OTG_STATE_A_PERIPHERAL:
				musb->ignore_disconnect = 0;
				del_timer(&musb->otg_timer);
				musb_g_reset(musb);
				break;
			case OTG_STATE_B_WAIT_ACON:
				dev_dbg(musb->controller, "HNP: RESET (%s), to b_peripheral\n",
					usb_otg_state_string(musb->xceiv->otg->state));
				musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
				musb_g_reset(musb);
				break;
			case OTG_STATE_B_IDLE:
				musb->xceiv->otg->state = OTG_STATE_B_PERIPHERAL;
				/* FALLTHROUGH */
			case OTG_STATE_B_PERIPHERAL:
				musb_g_reset(musb);
				break;
			default:
				dev_dbg(musb->controller, "Unhandled BUS RESET as %s\n",
					usb_otg_state_string(musb->xceiv->otg->state));
			}
		}
	}

	schedule_work(&musb->irq_work);

	return handled;
}

#ifdef EP_PROFILING
#define POLL_INTERVAL 10

unsigned int ep_prof[8][2];

static void ep_prof_work(struct work_struct *data)
{
	struct musb *musb = container_of(to_delayed_work(data), struct musb, ep_prof_work);

	int i;
	int tx = 0;
	int rx = 0;
	bool is_print = false;

	for (i = 1; i < 9; i++) {
		if ((ep_prof[i - 1][0] != 0) || (ep_prof[i - 1][1] != 0)) {
			os_printk(K_INFO, "[%d]T%d,R%d", i, ep_prof[i - 1][0], ep_prof[i - 1][1]);
			tx += ep_prof[i - 1][0];
			rx += ep_prof[i - 1][1];
			is_print = true;
		}
		ep_prof[i - 1][0] = ep_prof[i - 1][1] = 0;
	}

	if (is_print)
		os_printk(K_INFO, "T%d,R%d\n", tx, rx);

	schedule_delayed_work(&musb->ep_prof_work, msecs_to_jiffies(POLL_INTERVAL * 1000));
}
#endif

static void musb_restore_context(struct musb *musb);
static void musb_save_context(struct musb *musb);

/*-------------------------------------------------------------------------*/
/*
* Program the HDRC to start (enable interrupts, dma, etc.).
*/
void musb_start(struct musb *musb)
{
	u8 devctl = (u8) os_readl(U3D_DEVICE_CONTROL);

	os_printk(K_INFO, "%s  <== devctl %02x\n", __func__, devctl);

	if (musb->is_clk_on == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Recovert PHY. And turn on CLK. */
#ifdef CONFIG_PHY_MTK_SSUSB
		phy_power_on(musb->mtk_phy);
#else
		usb_phy_recover(musb->is_clk_on);
#endif
		musb->is_clk_on = 1;

		/* USB 2.0 slew rate calibration */
#ifndef CONFIG_PHY_MTK_SSUSB
		u3phy_ops->u2_slew_rate_calibration(u3phy);
#endif
#endif

		/* disable IP reset and power down, disable U2/U3 ip power down */
		_ex_mu3d_hal_ssusb_en();

		/* USB PLL Force settings */
#ifdef CONFIG_PROJECT_PHY
		usb20_pll_settings(false, false);
#endif

		/* reset U3D all dev module. */
		mu3d_hal_rst_dev();

		/*
		 * SW workaround of SSUSB device mode fake disable interrupt
		 * 1. Clear SSUSB_U3_PORT_DIS @ _ex_mu3d_hal_ssusb_en()
		 * 2. Wait SSUSB_U3_MAC_RST_B_STS change to 1. @ mu3d_hal_check_clk_sts()
		 * 3. Delay 50us
		 * 4. Clear U3 interrupt @ mu3d_hal_check_clk_sts()
		 * Recommended value : 50us
		 */
		udelay(20);

		musb_restore_context(musb);

		mu3d_reset_gpd_resource();
	}

	/*Enable Level 1 interrupt (BMU, QMU, MAC3, DMA, MAC2, EPCTL) */
	os_writel(U3D_LV1IESR, 0xFFFFFFFF);

	/* Initialize the default interrupts */
	_ex_mu3d_hal_system_intr_en();

#ifdef USB_GADGET_SUPERSPEED
	/* HS/FS detected by HW */
	/* USB2.0 controller will negotiate for HS mode when the device is reset by the host */
	os_writel(U3D_POWER_MANAGEMENT, (os_readl(U3D_POWER_MANAGEMENT) | HS_ENABLE));

	/* set LPM remote wake up enable by HW */
	os_writel(U3D_POWER_MANAGEMENT, (os_readl(U3D_POWER_MANAGEMENT) | LPM_HRWE));
	os_writel(U3D_USB2_EPCTL_LPM, (L1_EXIT_EP0_CHK | L1_EXIT_EP_IN_CHK | L1_EXIT_EP_OUT_CHK));
	os_writel(U3D_USB2_EPCTL_LPM_FC_CHK,
		  (L1_EXIT_EP0_FC_CHK | L1_EXIT_EP_IN_FC_CHK | L1_EXIT_EP_OUT_FC_CHK));

#ifdef CONFIG_USBIF_COMPLIANCE
	/* Accept LGO_U1/U2 at beginning */
	os_writel(U3D_LINK_POWER_CONTROL,
		  os_readl(U3D_LINK_POWER_CONTROL) | SW_U1_ACCEPT_ENABLE | SW_U2_ACCEPT_ENABLE);

	/* 3us timeout for PENDING HP */
	os_writel(U3D_LINK_HP_TIMER, (os_readl(U3D_LINK_HP_TIMER) & ~(PHP_TIMEOUT_VALUE)) | 0x6);

	/* set vbus force enable */
	os_setmsk(U3D_MISC_CTRL, (VBUS_FRC_EN | VBUS_ON));
#endif

	/* device responses to u3_exit from host automatically */
	os_writel(U3D_LTSSM_CTRL, os_readl(U3D_LTSSM_CTRL) & ~SOFT_U3_EXIT_EN);

#else
#ifdef USB_GADGET_DUALSPEED
	/* HS/FS detected by HW */
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) | HS_ENABLE);
#else
	/* FS only */
	os_writel(U3D_POWER_MANAGEMENT, os_readl(U3D_POWER_MANAGEMENT) & ~HS_ENABLE);
#endif
	/* disable U3 port */
	mu3d_hal_u3dev_dis();
#endif

	os_writel(U3D_LINK_RESET_INFO, os_readl(U3D_LINK_RESET_INFO) & ~WTCHRP);

	/* U2/U3 detected by HW */
	os_writel(U3D_DEVICE_CONF, 0);

	musb->is_active = 1;

	musb_platform_enable(musb);

#ifdef EP_PROFILING
	if (is_prof != 0)
		schedule_delayed_work(&musb->ep_prof_work, msecs_to_jiffies(POLL_INTERVAL * 1000));
#endif

	if (musb->softconnect) {
#ifdef SUPPORT_U3
		if (musb_speed && (musb->charger_mode == STANDARD_HOST))
			mu3d_hal_u3dev_en();
		else
			mu3d_hal_u2dev_connect();
#else
		mu3d_hal_u2dev_connect();
#endif
	}
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	mt_usb_dual_role_to_device();
#endif
}


static void musb_generic_disable(void)
{
	/*Disable interrupts */
	mu3d_hal_initr_dis();

	/*Clear all interrupt status */
	mu3d_hal_clear_intr();
}

/*
 * 1. Disable U2 & U3 function
 * 2. Notify disconnect event to upper
 */
static void gadget_stop(struct musb *musb)
{
	/* Disable U2 detect */
	mu3d_hal_u3dev_dis();
	mu3d_hal_u2dev_disconn();

	/* notify gadget driver */
	if (musb->g.speed != USB_SPEED_UNKNOWN) {
		if (musb->gadget_driver && musb->gadget_driver->disconnect)
			musb->gadget_driver->disconnect(&musb->g);
		musb->g.speed = USB_SPEED_UNKNOWN;
	}
}

static void set_ssusb_ip_sleep(struct musb *musb)
{
	/* Set below sequence to avoid power leakage */
#ifdef SUPPORT_U3
	if (musb_speed)
		os_setmsk(U3D_SSUSB_U3_CTRL_0P, SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
#endif
	os_setmsk(U3D_SSUSB_U2_CTRL_0P, SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
	os_setmsk(U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	os_setmsk(U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);
	udelay(50);
	os_setmsk(U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}

/* add start for XHCI */
void musb_power_down(struct musb *musb)
{
#ifdef EP_PROFILING
		cancel_delayed_work_sync(&musb->ep_prof_work);
#endif
		/*
		 * Note: musb_save_context() _MUST_ be called
		 * _BEFORE_ setting SSUSB_IP_SW_RST.
		 * Because when setting SSUSB_IP_SW_RST to reset the SSUSB IP,
		 * All MAC regs can _NOT_ be read and be reset to
		 * the default value.
		 * So save the MUST-SAVED reg in the context structure.
		 */
		musb_save_context(musb);

		set_ssusb_ip_sleep(musb);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Let PHY enter savecurrent mode. And turn off CLK. */
#ifdef CONFIG_PHY_MTK_SSUSB
		phy_power_off(musb->mtk_phy);
#else
		usb_phy_savecurrent(musb->is_clk_on);
#endif
		musb->is_clk_on = 0;
#endif
}
/* add end for XHCI */

/*
 * Make the HDRC stop (disable interrupts, etc.);
 * reversible by musb_start
 * called on gadget driver unregister
 * with controller locked, irqs blocked
 * acts as a NOP unless some role activated the hardware
 */
void musb_stop(struct musb *musb)
{
	os_printk(K_INFO, "musb_stop\n");
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	mt_usb_dual_role_to_none();
#endif

	/* stop IRQs, timers, ... */
	musb_platform_disable(musb);
	musb_generic_disable();

	/*Added by M */
	gadget_stop(musb);
	musb->is_active = 0;
	/*Added by M */
#ifndef CONFIG_USBIF_COMPLIANCE
	cancel_delayed_work_sync(&musb->check_ltssm_work);
#endif

	dev_dbg(musb->controller, "HDRC disabled\n");

	if (musb->active_ep == 0)
		queue_work(musb->st_wq, &musb->suspend_work);

	/* Move to suspend work queue */
#ifdef NEVER
	/*
	 * Note: When reset the SSUSB IP, All MAC regs can _NOT_ be accessed and be reset to the default value.
	 * So save the MUST-SAVED reg in the context structure before set SSUSB_IP_SW_RST.
	 */
	musb_save_context(musb);

	/* Set SSUSB_IP_SW_RST to avoid power leakage */
#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode)
		set_ssusb_ip_sleep(musb);
#else
	set_ssusb_ip_sleep(musb);
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
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

static void musb_shutdown(struct platform_device *pdev)
{
	struct musb *musb = dev_to_musb(&pdev->dev);
	unsigned long flags;

	pm_runtime_get_sync(musb->controller);
	spin_lock_irqsave(&musb->lock, flags);
	musb_platform_disable(musb);
	musb_generic_disable();
	spin_unlock_irqrestore(&musb->lock, flags);

#ifndef CONFIG_USBIF_COMPLIANCE
	if (!is_otg_enabled(musb) && is_host_enabled(musb))
		usb_remove_hcd(musb_to_hcd(musb));
#endif

	os_writel(U3D_DEVICE_CONTROL, 0);
	musb_platform_exit(musb);

	pm_runtime_put(musb->controller);
	/* FIXME power down */
}


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
#if defined(CONFIG_USB_MUSB_TUSB6010)			\
	|| defined(CONFIG_USB_MUSB_TUSB6010_MODULE)	\
	|| defined(CONFIG_USB_MUSB_OMAP2PLUS)		\
	|| defined(CONFIG_USB_MUSB_OMAP2PLUS_MODULE)	\
	|| defined(CONFIG_USB_MUSB_AM35X)		\
	|| defined(CONFIG_USB_MUSB_AM35X_MODULE)

#ifdef CONFIG_USBIF_COMPLIANCE
static ushort fifo_mode = 4;
#else
static ushort fifo_mode __initdata = 4;
#endif

#elif defined(CONFIG_USB_MUSB_UX500)			\
	|| defined(CONFIG_USB_MUSB_UX500_MODULE)

#ifdef CONFIG_USBIF_COMPLIANCE
static ushort fifo_mode = 5;
#else
static ushort fifo_mode __initdata = 5;
#endif

#else

#ifdef CONFIG_USBIF_COMPLIANCE
static ushort fifo_mode = 2;
#else
static ushort fifo_mode __initdata = 2;
#endif

#endif

/* "modprobe ... fifo_mode=1" etc */
module_param(fifo_mode, ushort, 0);
MODULE_PARM_DESC(fifo_mode, "initial endpoint configuration");

/*
 * tables defining fifo_mode values.  define more if you like.
 * for host side, make sure both halves of ep1 are set up.
 */

/* mode 0 - fits in 2KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_0_cfg[] = {
#else
static struct musb_fifo_cfg mode_0_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_RXTX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256,},
	{.hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256,},
};

/* mode 1 - fits in 4KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_1_cfg[] = {
#else
static struct musb_fifo_cfg mode_1_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512, .mode = BUF_DOUBLE,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512, .mode = BUF_DOUBLE,},
	{.hw_ep_num = 2, .style = FIFO_RXTX, .maxpacket = 512, .mode = BUF_DOUBLE,},
	{.hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256,},
	{.hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256,},
};

/* mode 2 - fits in 4KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_2_cfg[] = {
#else
static struct musb_fifo_cfg mode_2_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256,},
	{.hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256,},
};

/* mode 3 - fits in 4KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_3_cfg[] = {
#else
static struct musb_fifo_cfg mode_3_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512, .mode = BUF_DOUBLE,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512, .mode = BUF_DOUBLE,},
	{.hw_ep_num = 2, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256,},
	{.hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256,},
};

/* mode 4 - fits in 16KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_4_cfg[] = {
#else
static struct musb_fifo_cfg mode_4_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 4, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 4, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 5, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 5, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 6, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 6, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 7, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 7, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 8, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 8, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 9, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 9, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 10, .style = FIFO_TX, .maxpacket = 256,},
	{.hw_ep_num = 10, .style = FIFO_RX, .maxpacket = 64,},
	{.hw_ep_num = 11, .style = FIFO_TX, .maxpacket = 256,},
	{.hw_ep_num = 11, .style = FIFO_RX, .maxpacket = 64,},
	{.hw_ep_num = 12, .style = FIFO_TX, .maxpacket = 256,},
	{.hw_ep_num = 12, .style = FIFO_RX, .maxpacket = 64,},
	{.hw_ep_num = 13, .style = FIFO_RXTX, .maxpacket = 4096,},
	{.hw_ep_num = 14, .style = FIFO_RXTX, .maxpacket = 1024,},
	{.hw_ep_num = 15, .style = FIFO_RXTX, .maxpacket = 1024,},
};

/* mode 5 - fits in 8KB */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb_fifo_cfg mode_5_cfg[] = {
#else
static struct musb_fifo_cfg mode_5_cfg[] __initdata = {
#endif
	{.hw_ep_num = 1, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 1, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 2, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 3, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 4, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 4, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 5, .style = FIFO_TX, .maxpacket = 512,},
	{.hw_ep_num = 5, .style = FIFO_RX, .maxpacket = 512,},
	{.hw_ep_num = 6, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 6, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 7, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 7, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 8, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 8, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 9, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 9, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 10, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 10, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 11, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 11, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 12, .style = FIFO_TX, .maxpacket = 32,},
	{.hw_ep_num = 12, .style = FIFO_RX, .maxpacket = 32,},
	{.hw_ep_num = 13, .style = FIFO_RXTX, .maxpacket = 512,},
	{.hw_ep_num = 14, .style = FIFO_RXTX, .maxpacket = 1024,},
	{.hw_ep_num = 15, .style = FIFO_RXTX, .maxpacket = 1024,},
};

void ep0_setup(struct musb *musb, struct musb_hw_ep *hw_ep0, const struct musb_fifo_cfg *cfg)
{
	os_printk(K_INFO, "ep0_setup maxpacket: %d\n", cfg->maxpacket);

	hw_ep0->fifoaddr_rx = 0;
	hw_ep0->fifoaddr_tx = 0;
	hw_ep0->is_shared_fifo = true;
	hw_ep0->fifo = (void __iomem *)(uintptr_t) MUSB_FIFO_OFFSET(0);	/* QMU GPD address --> CPU DMA address */

	/* for U2 */
	hw_ep0->max_packet_sz_tx = cfg->maxpacket;
	hw_ep0->max_packet_sz_rx = cfg->maxpacket;

	/* Defines the maximum amount of data that can be transferred through EP0 in a single operation. */
	os_writelmskumsk(U3D_EP0CSR, hw_ep0->max_packet_sz_tx, EP0_MAXPKTSZ0, EP0_W1C_BITS);

	/* Enable EP0 interrupt */
	os_writel(U3D_EPIESR, os_readl(U3D_EPIESR) | EP0ISR);
}

/*
 * configure a fifo; for non-shared endpoints, this may be called
 * once for a tx fifo and once for an rx fifo.
 *
 * returns negative errno or offset for next fifo.
 */
#ifdef CONFIG_USBIF_COMPLIANCE
static int fifo_setup(struct musb *musb, struct musb_hw_ep *hw_ep, const struct musb_fifo_cfg *cfg,
		      u16 offset)
#else
static int __init
fifo_setup(struct musb *musb, struct musb_hw_ep *hw_ep, const struct musb_fifo_cfg *cfg, u16 offset)
#endif
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
	if (hw_ep->epnum == 1)
		musb->bulk_ep = hw_ep;
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

#ifdef CONFIG_USBIF_COMPLIANCE
static int ep_config_from_table(struct musb *musb)
#else
static int __init ep_config_from_table(struct musb *musb)
#endif
{
	const struct musb_fifo_cfg *cfg;
	unsigned i, n;
	int offset = 0;
	struct musb_hw_ep *hw_ep = musb->endpoints;

	if (musb->config->fifo_cfg) {
		cfg = musb->config->fifo_cfg;
		n = musb->config->fifo_cfg_size;
		os_printk(K_DEBUG, "%s: usb pre-cfg fifo_mode cfg=%p sz=%d\n", musb_driver_name,
			  cfg, n);
		goto done;
	}

	switch (fifo_mode) {
	default:
		fifo_mode = 0;
		/* FALLTHROUGH */
	case 0:
		cfg = mode_0_cfg;
		n = ARRAY_SIZE(mode_0_cfg);
		break;
	case 1:
		cfg = mode_1_cfg;
		n = ARRAY_SIZE(mode_1_cfg);
		break;
	case 2:
		cfg = mode_2_cfg;
		n = ARRAY_SIZE(mode_2_cfg);
		break;
	case 3:
		cfg = mode_3_cfg;
		n = ARRAY_SIZE(mode_3_cfg);
		break;
	case 4:
		cfg = mode_4_cfg;
		n = ARRAY_SIZE(mode_4_cfg);
		break;
	case 5:
		cfg = mode_5_cfg;
		n = ARRAY_SIZE(mode_5_cfg);
		break;
	}

	os_printk(K_INFO, "%s: setup fifo_mode %d\n", musb_driver_name, fifo_mode);


done:
#ifdef USB_GADGET_SUPERSPEED	/* SS */
	/* use SS EP0 as default; it may be changed later */
	os_printk(K_INFO, "%s ep_config_from_table ep0_cfg_u3\n", __func__);
	ep0_setup(musb, hw_ep, &ep0_cfg_u3);
#else				/* HS, FS */
	os_printk(K_INFO, "%s ep_config_from_table ep0_cfg_u2\n", __func__);
	ep0_setup(musb, hw_ep, &ep0_cfg_u2);
#endif
	/* assert(offset > 0) */

	/* NOTE:  for RTL versions >= 1.400 EPINFO and RAMINFO would
	 * be better than static musb->config->num_eps and DYN_FIFO_SIZE...
	 */

	for (i = 0; i < n; i++) {
		u8 epn = cfg->hw_ep_num;

		if (epn >= musb->config->num_eps) {
			os_printk(K_ERR, "%s: invalid ep %d\n", musb_driver_name, epn);
			return -EINVAL;
		}
		offset = fifo_setup(musb, hw_ep + epn, cfg++, offset);
		if (offset < 0) {
			os_printk(K_ERR, "%s: mem overrun, ep %d\n", musb_driver_name, epn);
			return -EINVAL;
		}
		epn++;
		musb->nr_endpoints = max(epn, musb->nr_endpoints);
	}

	os_printk(K_INFO, "%s: %d/%d max ep, %d/%d memory\n",
		  musb_driver_name,
		  n + 1, musb->config->num_eps * 2 - 1,
		  offset, (1 << (musb->config->ram_bits + 2)));

	if (!musb->bulk_ep) {
		pr_debug("%s: missing bulk\n", musb_driver_name);
		return -EINVAL;
	}

	return 0;
}


/*
 * ep_config_from_hw - when MUSB_C_DYNFIFO_DEF is false
 * @param musb the controller
 */
static int ep_config_from_hw(struct musb *musb)
{
	u8 epnum = 0;
	struct musb_hw_ep *hw_ep;
	void __iomem *mbase = musb->mregs;
	int ret = 0;

	dev_dbg(musb->controller, "<== static silicon ep config\n");

	/* FIXME pick up ep0 maxpacket size */

	for (epnum = 1; epnum < musb->config->num_eps; epnum++) {
		musb_ep_select(mbase, epnum);
		hw_ep = musb->endpoints + epnum;

		ret = musb_read_fifosize(musb, hw_ep, epnum);
		if (ret < 0)
			break;

		/* FIXME set up hw_ep->{rx,tx}_double_buffered */

		/* pick an RX/TX endpoint for bulk */
		if (hw_ep->max_packet_sz_tx < 512 || hw_ep->max_packet_sz_rx < 512)
			continue;

		/* REVISIT:  this algorithm is lazy, we should at least
		 * try to pick a double buffered endpoint.
		 */
		if (musb->bulk_ep)
			continue;
		musb->bulk_ep = hw_ep;
	}

	if (!musb->bulk_ep) {
		pr_debug("%s: missing bulk\n", musb_driver_name);
		return -EINVAL;
	}

	return 0;
}


enum { MUSB_CONTROLLER_MHDRC, MUSB_CONTROLLER_HDRC, };

/* Initialize MUSB (M)HDRC part of the USB hardware subsystem;
 * configure endpoints, or take their config from silicon
 */
#ifdef CONFIG_USBIF_COMPLIANCE
static int musb_core_init(u16 musb_type, struct musb *musb)
#else
static int __init musb_core_init(u16 musb_type, struct musb *musb)
#endif
{
	/* u8 reg; */
	/* char *type; */
	/* char aInfo[90], aRevision[32], aDate[12]; */
	void __iomem *mbase = musb->mregs;
	int status = 0;
	int i;

	musb->hwvers = os_readl(U3D_SSUSB_HW_ID);

	os_printk(K_INFO, "%s: HDC version %d\n", musb_driver_name, musb->hwvers);

	/* add for U3D */
	musb->txfifoadd_offset = U3D_FIFO_START_ADDRESS;
	musb->rxfifoadd_offset = U3D_FIFO_START_ADDRESS;

	os_printk(K_INFO, "%s EPnFIFOSz Tx=%x, Rx=%x\n", __func__, os_readl(U3D_CAP_EPNTXFFSZ),
		  os_readl(U3D_CAP_EPNRXFFSZ));
	os_printk(K_INFO, "%s EPnNum Tx=%x, Rx=%d\n", __func__, os_readl(U3D_CAP_EPINFO) & 0x1F,
		  (os_readl(U3D_CAP_EPINFO) >> 8) & 0x1F);

	if (os_readl(U3D_CAP_EPNTXFFSZ) && os_readl(U3D_CAP_EPNRXFFSZ))
		musb->dyn_fifo = true;
	else
#ifdef CONFIG_MTK_UART_USB_SWITCH
		musb->dyn_fifo = true;
#else
		musb->dyn_fifo = false;
#endif

	/* discover endpoint configuration */
	musb->nr_endpoints = 1;
	musb->epmask = 1;

	/* status = ep_config_from_table(musb); */
	if (musb->dyn_fifo)
		status = ep_config_from_table(musb);
	else
		status = ep_config_from_hw(musb);

	if (status < 0)
		return status;

	/* finish init, and print endpoint config */
	for (i = 0; i < musb->nr_endpoints; i++) {
		struct musb_hw_ep *hw_ep = musb->endpoints + i;

		hw_ep->fifo = (void __iomem *)(uintptr_t) MUSB_FIFO_OFFSET(i);
#ifdef CONFIG_USB_MUSB_TUSB6010
		hw_ep->fifo_async = musb->async + 0x400 + MUSB_FIFO_OFFSET(i);
		hw_ep->fifo_sync = musb->sync + 0x400 + MUSB_FIFO_OFFSET(i);
		hw_ep->fifo_sync_va = musb->sync_va + 0x400 + MUSB_FIFO_OFFSET(i);

		if (i == 0)
			hw_ep->conf = mbase - 0x400 + TUSB_EP0_CONF;
		else
			hw_ep->conf = mbase + 0x400 + (((i - 1) & 0xf) << 2);
#endif

		/* change data structure for ssusb */
		hw_ep->addr_txcsr0 = (void __iomem *)(uintptr_t) SSUSB_EP_TXCR0_OFFSET(i, 0);
		hw_ep->addr_txcsr1 = (void __iomem *)(uintptr_t) SSUSB_EP_TXCR1_OFFSET(i, 0);
		hw_ep->addr_txcsr2 = (void __iomem *)(uintptr_t) SSUSB_EP_TXCR2_OFFSET(i, 0);
		hw_ep->addr_rxcsr0 = (void __iomem *)(uintptr_t) SSUSB_EP_RXCR0_OFFSET(i, 0);
		hw_ep->addr_rxcsr1 = (void __iomem *)(uintptr_t) SSUSB_EP_RXCR1_OFFSET(i, 0);
		hw_ep->addr_rxcsr2 = (void __iomem *)(uintptr_t) SSUSB_EP_RXCR2_OFFSET(i, 0);
		hw_ep->addr_rxcsr3 = (void __iomem *)(uintptr_t) SSUSB_EP_RXCR3_OFFSET(i, 0);

		hw_ep->target_regs = musb_read_target_reg_base(i, mbase);
		hw_ep->rx_reinit = 1;
		hw_ep->tx_reinit = 1;

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
	_ex_mu3d_hal_alloc_qmu_mem(musb->controller);
	/* Iniital QMU */
	_ex_mu3d_hal_init_qmu();

	musb_save_context(musb);
#if defined(CONFIG_USB_MU3D_DRV_36BIT)
	dma_set_mask_and_coherent(musb->controller, DMA_BIT_MASK(36));
	dma_set_coherent_mask(musb->controller, DMA_BIT_MASK(36));
#endif
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
	u8 devctl, power = 0;
#ifndef USE_SSUSB_QMU
	u32 reg = 0, ep_num = 0;
#endif

#ifdef POWER_SAVING_MODE
	if (!(os_readl(U3D_SSUSB_U2_CTRL_0P) & SSUSB_U2_PORT_PDN)) {
		devctl = (u8) os_readl(U3D_DEVICE_CONTROL);
		power = (u8) os_readl(U3D_POWER_MANAGEMENT);
	} else {
		devctl = 0;
		power = 0;
		musb->int_usb = 0;
	}
#else
	devctl = (u8) os_readl(U3D_DEVICE_CONTROL);
	power = (u8) os_readl(U3D_POWER_MANAGEMENT);
#endif

	if (unlikely(!musb->softconnect)) {
		os_printk(K_WARNIN, "!softconnect, IRQ %s usb%04x tx%04x rx%04x\n",
			(devctl & USB_DEVCTL_HOSTMODE) ? "host" : "peripheral",
			musb->int_usb, musb->int_tx, musb->int_rx);
		return IRQ_HANDLED;
	}

	/* dev_dbg(musb->controller, "** IRQ %s usb%04x tx%04x rx%04x\n", */
	os_printk(K_DEBUG, "IRQ %s usb%04x tx%04x rx%04x\n",
		  (devctl & USB_DEVCTL_HOSTMODE) ? "host" : "peripheral",
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
EXPORT_SYMBOL_GPL(musb_interrupt);

#ifndef CONFIG_USB_MU3D_PIO_ONLY
static bool use_dma = 1;

/* "modprobe ... use_dma=0" etc */
module_param(use_dma, bool, 0);
MODULE_PARM_DESC(use_dma, "enable/disable use of DMA");

void musb_dma_completion(struct musb *musb, u8 epnum, u8 transmit)
{
	u8 devctl = musb_readb(musb->mregs, MUSB_DEVCTL);

	/* called with controller lock already held */

	if (!epnum) {
#ifndef CONFIG_USB_TUSB_OMAP_DMA
		if (!is_cppi_enabled()) {
			/* endpoint 0 */
			if (devctl & MUSB_DEVCTL_HM)
				/* Do nothing, MUSB does _NOT_ support host */
				/* musb_h_ep0_irq(musb); */
				os_printk(K_DEBUG, "Call musb_h_ep0_irq(), AYKM???!!!\n");
			else
				musb_g_ep0_irq(musb);
		}
#endif
	} else {
		/* endpoints 1..15 */
		if (transmit) {
			if (devctl & MUSB_DEVCTL_HM) {
				if (is_host_capable())
					/* Do nothing, MUSB does _NOT_ support host */
					/* musb_host_tx(musb, epnum); */
					os_printk(K_DEBUG, "Call musb_host_tx(), AYKM???!!!\n");
			} else {
				if (is_peripheral_capable())
					musb_g_tx(musb, epnum);
			}
		} else {
			/* receive */
			if (devctl & MUSB_DEVCTL_HM) {
				if (is_host_capable())
					/* Do nothing, MUSB does _NOT_ support host */
					/* musb_host_tx(musb, epnum); */
					os_printk(K_DEBUG, "Call musb_host_tx(), AYKM???!!!\n");
			} else {
				if (is_peripheral_capable())
					musb_g_rx(musb, epnum);
			}
		}
	}
}

#else
#define use_dma			0
#endif

/*-------------------------------------------------------------------------*/

#ifdef CONFIG_SYSFS

static ssize_t musb_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct musb *musb = dev_to_musb(dev);
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&musb->lock, flags);
	ret = sprintf(buf, "%s\n", usb_otg_state_string(musb->xceiv->otg->state));
	spin_unlock_irqrestore(&musb->lock, flags);

	return ret;
}

static ssize_t
musb_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	struct musb *musb = dev_to_musb(dev);
	unsigned long flags;
	int status;

	spin_lock_irqsave(&musb->lock, flags);
	if (sysfs_streq(buf, "host"))
		status = musb_platform_set_mode(musb, MUSB_HOST);
	else if (sysfs_streq(buf, "peripheral"))
		status = musb_platform_set_mode(musb, MUSB_PERIPHERAL);
	else if (sysfs_streq(buf, "otg"))
		status = musb_platform_set_mode(musb, MUSB_OTG);
	else
		status = -EINVAL;
	spin_unlock_irqrestore(&musb->lock, flags);

	return (status == 0) ? n : status;
}

static DEVICE_ATTR(mode, 0644, musb_mode_show, musb_mode_store);

static ssize_t
musb_vbus_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	struct musb *musb = dev_to_musb(dev);
	unsigned long flags;
	unsigned long val;

	/*if (sscanf(buf, "%lu", &val) < 1) {*/
	if (kstrtol(buf, 10, &val) < 1) {
		dev_err(dev, "Invalid VBUS timeout ms value\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&musb->lock, flags);
	/* force T(a_wait_bcon) to be zero/unlimited *OR* valid */
	musb->a_wait_bcon = val ? max_t(int, val, OTG_TIME_A_WAIT_BCON) : 0;
	if (musb->xceiv->otg->state == OTG_STATE_A_WAIT_BCON)
		musb->is_active = 0;
	musb_platform_try_idle(musb, jiffies + msecs_to_jiffies(val));
	spin_unlock_irqrestore(&musb->lock, flags);

	return n;
}

static ssize_t musb_vbus_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct musb *musb = dev_to_musb(dev);
	unsigned long flags;
	unsigned long val;
	int vbus;

	spin_lock_irqsave(&musb->lock, flags);
	val = musb->a_wait_bcon;
	/* FIXME get_vbus_status() is normally #defined as false...
	 * and is effectively TUSB-specific.
	 */
	vbus = musb_platform_get_vbus_status(musb);
	spin_unlock_irqrestore(&musb->lock, flags);

	return sprintf(buf, "Vbus %s, timeout %lu msec\n", vbus ? "on" : "off", val);
}

static DEVICE_ATTR(vbus, 0644, musb_vbus_show, musb_vbus_store);

/* Gadget drivers can't know that a host is connected so they might want
 * to start SRP, but users can.  This allows userspace to trigger SRP.
 */
static ssize_t
musb_srp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	struct musb *musb = dev_to_musb(dev);
	unsigned long srp;

	/*if (sscanf(buf, "%hu", &srp) != 1 || (srp != 1)) {*/
	if (kstrtol(buf, 10, &srp) != 1 || (srp != 1)) {
		dev_err(dev, "SRP: Value must be 1\n");
		return -EINVAL;
	}

	if (srp == 1)
		musb_g_wakeup(musb);

	return n;
}

static DEVICE_ATTR(srp, 0644, NULL, musb_srp_store);
DEVICE_ATTR(cmode, 0664, musb_cmode_show, musb_cmode_store);
DEVICE_ATTR(saving, 0664, musb_saving_mode_show, musb_saving_mode_store);


#ifdef CONFIG_MTK_UART_USB_SWITCH
DEVICE_ATTR(portmode, 0664, musb_portmode_show, musb_portmode_store);
DEVICE_ATTR(tx, 0664, musb_tx_show, musb_tx_store);
DEVICE_ATTR(rx, 0444, musb_rx_show, NULL);
DEVICE_ATTR(uartpath, 0444, musb_uart_path_show, NULL);
#endif

#ifdef CONFIG_MTK_SIB_USB_SWITCH
DEVICE_ATTR(sib_enable, 0664, musb_sib_enable_show, musb_sib_enable_store);
#endif

static struct attribute *musb_attributes[] = {
	&dev_attr_mode.attr,
	&dev_attr_vbus.attr,
	&dev_attr_srp.attr,
	&dev_attr_cmode.attr,
	&dev_attr_saving.attr,
#ifdef CONFIG_MTK_UART_USB_SWITCH
	&dev_attr_portmode.attr,
	&dev_attr_tx.attr,
	&dev_attr_rx.attr,
	&dev_attr_uartpath.attr,
#endif
#ifdef CONFIG_MTK_SIB_USB_SWITCH
	&dev_attr_sib_enable.attr,
#endif
	NULL
};

static const struct attribute_group musb_attr_group = {
	.attrs = musb_attributes,
};

#endif				/* sysfs */

static void musb_save_context(struct musb *musb)
{
#if 1
	os_printk(K_INFO, "SKIP %s\n", __func__);
#else
	int i;

	for (i = 0; i < musb->config->num_eps; ++i) {
		os_printk(K_DEBUG, "%s EP%d\n", __func__, i);
#ifdef USE_SSUSB_QMU
		/* Save TXQ/RXQ starting address. Those would be reset to 0 after reset SSUSB IP. */
		musb->context.index_regs[i].txqmuaddr = os_readl(USB_QMU_TQSAR(i + 1));
		os_printk(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			  musb->context.index_regs[i].txqmuaddr);
		musb->context.index_regs[i].rxqmuaddr = os_readl(USB_QMU_RQSAR(i + 1));
		os_printk(K_DEBUG, "%s RQSAR[%d]=%x\n", __func__, i,
			  musb->context.index_regs[i].rxqmuaddr);
#endif
	}
#endif
}

static void musb_restore_context(struct musb *musb)
{
#if 1
	os_printk(K_INFO, "SKIP %s\n", __func__);
#else
	int i;

	for (i = 0; i < musb->config->num_eps; ++i) {
#ifdef USE_SSUSB_QMU
		os_writel(USB_QMU_TQSAR(i + 1), musb->context.index_regs[i].txqmuaddr);
		os_writel(USB_QMU_RQSAR(i + 1), musb->context.index_regs[i].rxqmuaddr);
		os_printk(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			  os_readl(USB_QMU_TQSAR(i + 1)));
		os_printk(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			  os_readl(USB_QMU_RQSAR(i + 1)));
#endif
	}
#endif
}

static void musb_suspend_work(struct work_struct *data)
{
	struct musb *musb = container_of(data, struct musb, suspend_work);

	os_printk(K_INFO, "%s active_ep=%d, clk_on=%d\n", __func__, musb->active_ep,
		  musb->is_clk_on);

	if (musb->is_clk_on == 1
	    && !usb_cable_connected()) {

#ifdef EP_PROFILING
		cancel_delayed_work_sync(&musb->ep_prof_work);
#endif
		/*
		 * Note: musb_save_context() _MUST_ be called _BEFORE_ setting SSUSB_IP_SW_RST.
		 * Because when setting SSUSB_IP_SW_RST to reset the SSUSB IP,
		 * All MAC regs can _NOT_ be read and be reset to the default value.
		 * So save the MUST-SAVED reg in the context structure.
		 */
		musb_save_context(musb);

		set_ssusb_ip_sleep(musb);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Let PHY enter savecurrent mode. And turn off CLK. */
#ifdef CONFIG_PHY_MTK_SSUSB
		phy_power_off(musb->mtk_phy);
#else
		usb_phy_savecurrent(musb->is_clk_on);
#endif
		musb->is_clk_on = 0;
#endif
	}
}

/* Only used to provide driver mode change events */
static void musb_irq_work(struct work_struct *data)
{
	struct musb *musb = container_of(data, struct musb, irq_work);
	static int old_state;

	os_printk(K_INFO, "%s [%d]=[%d]\n", __func__, musb->xceiv->otg->state, old_state);

	if (musb->xceiv->otg->state != old_state) {
		old_state = musb->xceiv->otg->state;
		sysfs_notify(&musb->controller->kobj, NULL, "mode");
	}
}

const struct hc_driver musb_hc_driver = {
	.description = "musb-hcd",
	.product_desc = "MUSB HDRC host driver",
	.hcd_priv_size = sizeof(struct musb),
	.flags = HCD_USB2 | HCD_MEMORY,
};

#ifdef CONFIG_USB_C_SWITCH
#ifndef CONFIG_TCPC_CLASS
static struct typec_switch_data switch_driver = {
	.name = (char *)musb_driver_name,
	.type = DEVICE_TYPE,
	.enable		= typec_switch_usb_connect,
	.disable	= typec_switch_usb_disconnect,
};
#endif /* CONFIG_TCPC_CLASS */
#endif
/* --------------------------------------------------------------------------
 * Init support
 */
#ifdef CONFIG_USBIF_COMPLIANCE
static struct musb *allocate_instance(struct device *dev,
				      struct musb_hdrc_config *config, void __iomem *mbase)
#else
static struct musb *__init
allocate_instance(struct device *dev, struct musb_hdrc_config *config, void __iomem *mbase)
#endif
{
	struct musb *musb;
	struct musb_hw_ep *ep;
	int epnum;
	struct usb_hcd *hcd;

	hcd = usb_create_hcd(&musb_hc_driver, dev, dev_name(dev));
	if (!hcd)
		return NULL;
	/* usbcore sets dev->driver_data to hcd, and sometimes uses that... */

	musb = hcd_to_musb(hcd);
	INIT_LIST_HEAD(&musb->control);
	INIT_LIST_HEAD(&musb->in_bulk);
	INIT_LIST_HEAD(&musb->out_bulk);

	hcd->uses_new_polling = 1;
	hcd->has_tt = 1;

	musb->vbuserr_retry = VBUSERR_RETRY_COUNT;
	musb->a_wait_bcon = OTG_TIME_A_WAIT_BCON;
	dev_set_drvdata(dev, musb);
	musb->mregs = mbase;
	musb->ctrl_base = mbase;
	musb->nIrq = -ENODEV;
	musb->config = config;
	WARN_ON(musb->config->num_eps > MUSB_C_NUM_EPS);
	for (epnum = 0, ep = musb->endpoints; epnum < musb->config->num_eps; epnum++, ep++) {
		ep->musb = musb;
		ep->epnum = epnum;
	}
	musb->in_ipo_off = false;
	musb->controller = dev;

	/* added for ssusb: */
	/* musb->xceiv = kzalloc(sizeof(struct otg_transceiver), GFP_KERNEL); */
	/* memset(musb->xceiv, 0, sizeof(struct otg_transceiver)); */
	/* musb->xceiv->otg->state = OTG_STATE_B_IDLE; //initial its value */

#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_PROJECT_PHY)
	if (usb20_phy_init_debugfs())
		os_printk(K_ERR, "usb20_phy_init_debugfs fail!\n");
#endif
#endif

	return musb;
}

static void musb_free(struct musb *musb)
{
	/* this has multiple entry modes. it handles fault cleanup after
	 * probe(), where things may be partially set up, as well as rmmod
	 * cleanup after everything's been de-activated.
	 */

#ifdef CONFIG_SYSFS
	sysfs_remove_group(&musb->controller->kobj, &musb_attr_group);
#endif

	musb_gadget_cleanup(musb);

	if (musb->nIrq >= 0) {
		if (musb->irq_wake)
			disable_irq_wake(musb->nIrq);
		free_irq(musb->nIrq, musb);
	}
#ifdef USE_SSUSB_QMU
	tasklet_kill(&musb->qmu_done);
	tasklet_kill(&musb->error_recovery);
#endif

	cancel_work_sync(&musb->irq_work);
	cancel_delayed_work_sync(&musb->connection_work);
	/* cancel_delayed_work_sync(&musb->check_ltssm_work); */
	cancel_work_sync(&musb->suspend_work);

#ifdef USE_SSUSB_QMU
	_ex_mu3d_hal_free_qmu_mem(musb->controller);
#endif
/*
 *	if (is_dma_capable() && musb->dma_controller) {
 *		struct dma_controller	*c = musb->dma_controller;
 *
 *		(void) c->stop(c);
 *		dma_controller_destroy(c);
 *	}
*/
	wakeup_source_trash(&musb->usb_wakelock);

	/* added for ssusb: */
#ifdef CONFIG_USBIF_COMPLIANCE
	/* kfree(musb->xceiv); //free the instance allocated in allocate_instance */
	/* musb->xceiv = NULL; */
	/* kfree(musb); */
#else
	/*i add these, need to test */
	usb_put_hcd(musb_to_hcd(musb));

	kfree(musb->xceiv);	/* free the instance allocated in allocate_instance */
	musb->xceiv = NULL;

	kfree(musb);
#endif

}

/*
 * Perform generic per-controller initialization.
 *
 * @pDevice: the controller (already clocked, etc)
 * @nIrq: irq
 * @mregs: virtual address of controller registers,
 *	not yet corrected for platform-specific offsets
 */
#ifdef CONFIG_USBIF_COMPLIANCE
static int musb_init_controller(struct device *dev, int nIrq, void __iomem *ctrl)
#else
static int __init musb_init_controller(struct device *dev, int nIrq, void __iomem *ctrl)
#endif
{
	int status;
	struct musb *musb;
	struct musb_hdrc_platform_data *plat = dev->platform_data;
#ifndef CONFIG_USBIF_COMPLIANCE
	struct usb_hcd *hcd;
#endif
	/* The driver might handle more features than the board; OK.
	 * Fail when the board needs a feature that's not enabled.
	 */

	os_printk(K_INFO, "[MU3D]%s\n", __func__);

	if (!plat) {
		dev_err(dev, "no platform_data?\n");
		status = -ENODEV;
		goto fail0;
	}

	/* allocate */
	musb = allocate_instance(dev, plat->config, ctrl);
	if (!musb) {
		status = -ENOMEM;
		goto fail0;
	}
	/* pm_runtime_use_autosuspend(musb->controller); */
	/* pm_runtime_set_autosuspend_delay(musb->controller, 200); */
	/* pm_runtime_enable(musb->controller); */

	spin_lock_init(&musb->lock);
	sema_init(&musb->musb_lock, 1);
	musb->board_mode = plat->mode;
	musb->board_set_power = plat->set_power;
	musb->min_power = plat->min_power;
	musb->ops = plat->platform_ops;
	musb->usb_mode = CABLE_MODE_NORMAL;

	_mu3d_musb = musb;

	wakeup_source_init(&musb->usb_wakelock, "USB.lock");

	INIT_DELAYED_WORK(&musb->connection_work, connection_work);

	INIT_DELAYED_WORK(&musb->check_ltssm_work, check_ltssm_work);

#ifndef CONFIG_USBIF_COMPLIANCE
	INIT_DELAYED_WORK(&musb->reconnect_work, reconnect_work);
#endif

#ifdef EP_PROFILING
	INIT_DELAYED_WORK(&musb->ep_prof_work, ep_prof_work);
#endif
	musb->st_wq = create_singlethread_workqueue("mu3d_st_wq");

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
	/* pm_runtime_get_sync(musb->controller); */

	/* ideally this would be abstracted in platform setup */
#ifdef USE_SSUSB_QMU
	if (!is_dma_capable())
#else
	if (!is_dma_capable() || !musb->dma_controller)
#endif
		dev->dma_mask = NULL;

	/* be sure interrupts are disabled before connecting ISR */
	musb_platform_disable(musb);
	musb_generic_disable();

	/* setup musb parts of the core (especially endpoints) */
	status = musb_core_init(plat->config->multipoint
				? MUSB_CONTROLLER_MHDRC : MUSB_CONTROLLER_HDRC, musb);
	if (status < 0)
		goto fail3;

#ifdef CONFIG_USB_C_SWITCH
#ifndef CONFIG_TCPC_CLASS
	switch_driver.priv_data = musb;
	os_printk(K_INFO, "type c test\n");
	status = register_typec_switch_callback(&switch_driver);
	if (status < 0)
		goto fail3;
#endif /* CONFIG_TCPC_CLASS */
#endif

	/* REVISIT-J: Do _NOT_ support OTG functionality */
	/* setup_timer(&musb->otg_timer, musb_otg_timer_func, (unsigned long) musb); */

	/* Init IRQ workqueue before request_irq */
	INIT_WORK(&musb->irq_work, musb_irq_work);

	INIT_WORK(&musb->suspend_work, musb_suspend_work);
#ifdef USE_SSUSB_QMU
	tasklet_init(&musb->qmu_done, qmu_done_tasklet, (unsigned long)musb);
	tasklet_init(&musb->error_recovery, qmu_error_recovery, (unsigned long)musb);
#endif

	/* attach to the IRQ */
	if (request_irq(nIrq, musb->isr, IRQF_TRIGGER_LOW, dev_name(dev), musb)) {
		dev_err(dev, "request_irq %d failed!\n", nIrq);
		status = -ENODEV;
		goto fail3;
	}
	musb->nIrq = nIrq;
	/* FIXME this handles wakeup irqs wrong */
	if (enable_irq_wake(nIrq) == 0) {
		musb->irq_wake = 1;
		device_init_wakeup(dev, 1);
	} else {
		musb->irq_wake = 0;
	}

	/* host side needs more setup */
#ifndef CONFIG_USBIF_COMPLIANCE
	if (is_host_enabled(musb)) {
		hcd = musb_to_hcd(musb);
		otg_set_host(musb->xceiv->otg, &hcd->self);

		if (is_otg_enabled(musb))
			hcd->self.otg_port = 1;

		musb->xceiv->otg->host = &hcd->self;
		hcd->power_budget = 2 * (plat->power ? : 250);

		/* program PHY to use external vBus if required */
		if (plat->extvbus) {
			u8 busctl = musb_read_ulpi_buscontrol(musb->mregs);

			busctl |= MUSB_ULPI_USE_EXTVBUS;
			musb_write_ulpi_buscontrol(musb->mregs, busctl);
		}
	}
#endif

	MUSB_DEV_MODE(musb);
	musb->xceiv->otg->default_a = 0;
	musb->xceiv->otg->state = OTG_STATE_B_IDLE;

	status = musb_gadget_setup(musb);

	if (status < 0)
		goto fail3;

	status = musb_init_debugfs(musb);
	if (status < 0)
		goto fail4;

#ifdef CONFIG_SYSFS
	status = sysfs_create_group(&musb->controller->kobj, &musb_attr_group);
	if (status)
		goto fail5;
#endif

	pm_runtime_put(musb->controller);

	dev_info(dev, "USB %s mode controller at %p using %s, IRQ %d\n", ({
			char *s;

			switch (musb->board_mode) {
			case MUSB_HOST:
			s = "Host"; break; case MUSB_PERIPHERAL:
			s = "Peripheral"; break; default:
			s = "OTG"; break; }; s; }
		), ctrl, (is_dma_capable() && musb->dma_controller)
		? "DMA" : "PIO", musb->nIrq);

	/* only enable on iddig mode */
#ifndef CONFIG_USB_C_SWITCH
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	mt_usb_dual_role_init(musb);
#endif
#endif

	return 0;

fail5:
	musb_exit_debugfs(musb);
fail4:
	if (!is_otg_enabled(musb) && is_host_enabled(musb))
		usb_remove_hcd(musb_to_hcd(musb));
	else
		musb_gadget_cleanup(musb);

fail3:
	if (musb->irq_wake)
		device_init_wakeup(dev, 0);
	musb_platform_exit(musb);

fail1:
	dev_err(musb->controller, "musb_init_controller failed with status %d\n", status);

	musb_free(musb);

fail0:

	return status;

}

#define USB3_BASE_REGS_ADDR_RES_NAME "ssusb_base"
#define USB3_IPPC_REGS_ADDR_RES_NAME "ssusb_ippc"
#define USB3_SIF2_REGS_ADDR_RES_NAME "ssusb_sif2"

static void __iomem *acquire_reg_base(struct platform_device *pdev, const char *res_name)
{
	struct resource *iomem;
	void __iomem *base = NULL;

	iomem = platform_get_resource_byname(pdev, IORESOURCE_MEM, res_name);
	if (!iomem) {
		pr_err("Can't get resource for %s\n", res_name);
		goto end;
	}
	os_printk(K_INFO, "iomem=0x%lx\n", (uintptr_t) iomem->start);

	base = ioremap(iomem->start, resource_size(iomem));
	if (!(uintptr_t) base) {
		pr_err("Can't remap %s\n", res_name);
		goto end;
	}
	os_printk(K_INFO, "%s=0x%lx\n", res_name, (uintptr_t) (base));
end:
	return base;
}

/*-------------------------------------------------------------------------*/

/* all implementations (PCI bridge to FPGA, VLYNQ, etc) should just
 * bridge to a platform device; this driver then suffices.
 */
#ifdef CONFIG_USBIF_COMPLIANCE
static int mu3d_normal_driver_on;

static int musb_probe(struct platform_device *pdev)
#else
static int __init musb_probe(struct platform_device *pdev)
#endif
{
	struct device *dev = &pdev->dev;
	int irq = 0;
	int status = 0;
	struct device_node *ap_pll_con0_node = NULL;
#ifdef CONFIG_MTK_UART_USB_SWITCH
	struct device_node *ap_uart0_node = NULL;
#endif

	os_printk(K_INFO, "[MU3D]musb_probe\n");

#ifdef SUPPORT_U3
#if defined(CONFIG_USB_MU3D_DEFAULT_U2_MODE) && !defined(U3_COMPLIANCE)
	musb_speed = musb_hal_speed = 0;
#else
	musb_speed = musb_hal_speed = 1;
#endif
#endif

	irq = irq_of_parse_and_map(dev->parent->of_node, 0);
	os_printk(K_INFO, "[MU3D]musb_probe irq=%d\n", irq);

	u3_base = acquire_reg_base(pdev, USB3_BASE_REGS_ADDR_RES_NAME);
	if (!u3_base)
		goto exit_regs;

	u3_ippc_base = acquire_reg_base(pdev, USB3_IPPC_REGS_ADDR_RES_NAME);
	if (!u3_ippc_base)
		goto exit_regs;

	u3_sif2_base = acquire_reg_base(pdev, USB3_SIF2_REGS_ADDR_RES_NAME);
	if (!u3_sif2_base)
		goto exit_regs;

#ifdef CONFIG_MTK_UART_USB_SWITCH
	ap_uart0_node = of_find_compatible_node(NULL, NULL, AP_UART0_COMPATIBLE_NAME);

	if (ap_uart0_node == NULL) {
		os_printk(K_ERR, "USB get ap_uart0_node failed\n");
		if (ap_uart0_base)
			iounmap(ap_uart0_base);
		ap_uart0_base = 0;
	} else {
		ap_uart0_base = of_iomap(ap_uart0_node, 0);
	}
#endif

	ap_pll_con0_node = of_find_compatible_node(NULL, NULL, AP_PLL_CON0_COMPATIBLE_NAME);

	if (ap_pll_con0_node == NULL) {
		os_printk(K_ERR, "USB get ap_pll_con0_node failed\n");
		if (ap_pll_con0)
			iounmap(ap_pll_con0);
		ap_pll_con0 = 0;
	} else {
		ap_pll_con0 = of_iomap(ap_pll_con0_node, 0);
	}

#ifdef CONFIG_FPGA_EARLY_PORTING
	if (!i2c_physical_base) {
		pr_err("no i2c_physical_base\n");
		WARN_ON(1);
	}

	i2c_base = ioremap(i2c_physical_base, 0x1000);

	if (!(i2c_base)) {
		pr_err("Can't remap I2C BASE\n");
		status = -ENOMEM;
	}
	os_printk(K_INFO, "I2C BASE=0x%lx, %x\n", (uintptr_t) (i2c_base), i2c_physical_base);
#endif

	status = musb_init_controller(dev, irq, u3_base);
	if (status < 0)
		goto exit_regs;

	return status;

exit_regs:
	if (u3_base)
		iounmap(u3_base);
	if (u3_ippc_base)
		iounmap(u3_ippc_base);
	if (u3_sif2_base)
		iounmap(u3_sif2_base);
	u3_base = 0;
	u3_ippc_base = 0;
	u3_sif2_base = 0;

	return status;
}

static int musb_remove(struct platform_device *pdev)
{
	struct musb *musb = dev_to_musb(&pdev->dev);
#ifndef CONFIG_USBIF_COMPLIANCE
	void __iomem *ctrl_base = musb->ctrl_base;
#endif
	/* this gets called on rmmod.
	 *  - Host mode: host may still be active
	 *  - Peripheral mode: peripheral is deactivated (or never-activated)
	 *  - OTG mode: both roles are deactivated (or never-activated)
	 */

#ifdef CONFIG_SYSFS		/* USBIF */
	sysfs_remove_group(&musb->controller->kobj, &musb_attr_group);
#endif
	pm_runtime_get_sync(musb->controller);
	musb_exit_debugfs(musb);
	musb_shutdown(pdev);

	pm_runtime_put(musb->controller);
	musb_free(musb);
	_mu3d_musb = NULL;
#ifndef CONFIG_USBIF_COMPLIANCE
	/* USB IF share resource with mu3d nor drv, so do not unmap it in IF case */
	iounmap(ctrl_base);
#endif
	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

/*
 * MU3D driver does _NOT_ use PM to control USB power state.
 * When cable disconnected and all EPs are disabled, turn off all the clock and powers.
 * Turn on all the clock and powers until the cable exists.
 */
#ifdef NEVER			/* CONFIG_PM */

/*Do _NOT_ use the original save and restore context functions*/
#ifdef NEVER
static void musb_save_context(struct musb *musb)
{
	int i;
	void __iomem *musb_base = musb->mregs;
	void __iomem *epio;

	if (is_host_enabled(musb)) {
		musb->context.frame = musb_readw(musb_base, MUSB_FRAME);
		musb->context.testmode = musb_readb(musb_base, MUSB_TESTMODE);
		musb->context.busctl = musb_read_ulpi_buscontrol(musb->mregs);
	}
	musb->context.power = musb_readb(musb_base, MUSB_POWER);
	musb->context.intrtxe = musb_readw(musb_base, MUSB_INTRTXE);
	musb->context.intrrxe = musb_readw(musb_base, MUSB_INTRRXE);
	musb->context.intrusbe = musb_readb(musb_base, MUSB_INTRUSBE);
	musb->context.index = musb_readb(musb_base, MUSB_INDEX);
	musb->context.devctl = musb_readb(musb_base, MUSB_DEVCTL);

	for (i = 0; i < musb->config->num_eps; ++i) {
		struct musb_hw_ep *hw_ep;

		hw_ep = &musb->endpoints[i];
		if (!hw_ep)
			continue;

		epio = hw_ep->regs;
		if (!epio)
			continue;

		musb->context.index_regs[i].txmaxp = musb_readw(epio, MUSB_TXMAXP);
		musb->context.index_regs[i].txcsr = musb_readw(epio, MUSB_TXCSR);
		musb->context.index_regs[i].rxmaxp = musb_readw(epio, MUSB_RXMAXP);
		musb->context.index_regs[i].rxcsr = musb_readw(epio, MUSB_RXCSR);

		if (musb->dyn_fifo) {
			musb->context.index_regs[i].txfifoadd = musb_read_txfifoadd(musb_base);
			musb->context.index_regs[i].rxfifoadd = musb_read_rxfifoadd(musb_base);
			musb->context.index_regs[i].txfifosz = musb_read_txfifosz(musb_base);
			musb->context.index_regs[i].rxfifosz = musb_read_rxfifosz(musb_base);
		}
		if (is_host_enabled(musb)) {
			musb->context.index_regs[i].txtype = musb_readb(epio, MUSB_TXTYPE);
			musb->context.index_regs[i].txinterval = musb_readb(epio, MUSB_TXINTERVAL);
			musb->context.index_regs[i].rxtype = musb_readb(epio, MUSB_RXTYPE);
			musb->context.index_regs[i].rxinterval = musb_readb(epio, MUSB_RXINTERVAL);

			musb->context.index_regs[i].txfunaddr = musb_read_txfunaddr(musb_base, i);
			musb->context.index_regs[i].txhubaddr = musb_read_txhubaddr(musb_base, i);
			musb->context.index_regs[i].txhubport = musb_read_txhubport(musb_base, i);

			musb->context.index_regs[i].rxfunaddr = musb_read_rxfunaddr(musb_base, i);
			musb->context.index_regs[i].rxhubaddr = musb_read_rxhubaddr(musb_base, i);
			musb->context.index_regs[i].rxhubport = musb_read_rxhubport(musb_base, i);
		}
	}
}

static void musb_restore_context(struct musb *musb)
{
	int i;
	void __iomem *musb_base = musb->mregs;
	void __iomem *ep_target_regs;
	void __iomem *epio;

	if (is_host_enabled(musb)) {
		musb_writew(musb_base, MUSB_FRAME, musb->context.frame);
		musb_writeb(musb_base, MUSB_TESTMODE, musb->context.testmode);
		musb_write_ulpi_buscontrol(musb->mregs, musb->context.busctl);
	}
	musb_writeb(musb_base, MUSB_POWER, musb->context.power);
	musb_writew(musb_base, MUSB_INTRTXE, musb->context.intrtxe);
	musb_writew(musb_base, MUSB_INTRRXE, musb->context.intrrxe);
	musb_writeb(musb_base, MUSB_INTRUSBE, musb->context.intrusbe);
	musb_writeb(musb_base, MUSB_DEVCTL, musb->context.devctl);

	for (i = 0; i < musb->config->num_eps; ++i) {
		struct musb_hw_ep *hw_ep;

		hw_ep = &musb->endpoints[i];
		if (!hw_ep)
			continue;

		epio = hw_ep->regs;
		if (!epio)
			continue;

		musb_writew(epio, MUSB_TXMAXP, musb->context.index_regs[i].txmaxp);
		musb_writew(epio, MUSB_TXCSR, musb->context.index_regs[i].txcsr);
		musb_writew(epio, MUSB_RXMAXP, musb->context.index_regs[i].rxmaxp);
		musb_writew(epio, MUSB_RXCSR, musb->context.index_regs[i].rxcsr);

		if (musb->dyn_fifo) {
			musb_write_txfifosz(musb_base, musb->context.index_regs[i].txfifosz);
			musb_write_rxfifosz(musb_base, musb->context.index_regs[i].rxfifosz);
			musb_write_txfifoadd(musb_base, musb->context.index_regs[i].txfifoadd);
			musb_write_rxfifoadd(musb_base, musb->context.index_regs[i].rxfifoadd);
		}

		if (is_host_enabled(musb)) {
			musb_writeb(epio, MUSB_TXTYPE, musb->context.index_regs[i].txtype);
			musb_writeb(epio, MUSB_TXINTERVAL, musb->context.index_regs[i].txinterval);
			musb_writeb(epio, MUSB_RXTYPE, musb->context.index_regs[i].rxtype);
			musb_writeb(epio, MUSB_RXINTERVAL, musb->context.index_regs[i].rxinterval);
			musb_write_txfunaddr(musb_base, i, musb->context.index_regs[i].txfunaddr);
			musb_write_txhubaddr(musb_base, i, musb->context.index_regs[i].txhubaddr);
			musb_write_txhubport(musb_base, i, musb->context.index_regs[i].txhubport);

			ep_target_regs = musb_read_target_reg_base(i, musb_base);

			musb_write_rxfunaddr(ep_target_regs, musb->context.index_regs[i].rxfunaddr);
			musb_write_rxhubaddr(ep_target_regs, musb->context.index_regs[i].rxhubaddr);
			musb_write_rxhubport(ep_target_regs, musb->context.index_regs[i].rxhubport);
		}
	}
	musb_writeb(musb_base, MUSB_INDEX, musb->context.index);
}
#endif				/* NEVER */

static void musb_save_context(struct musb *musb)
{
	int i;

#ifdef CONFIG_USB_MU3D_DRV
	/*
	 * U3D_EPIER(Endpoint 0 interrupt enable.) and U3D_EP0CSR(EP0 MaxP Size)
	 * would be configured at
	 * #U2: RESET Signal -> musb_g_reset() -> musb_conifg_ep0() -> ep0_setup()
	 * #U3: ENTER_U0_INTR -> musb_conifg_ep0() -> ep0_setup()
	 * U3D_EPIER(Endpoint N interrupt enable.) at PIO mode
	 * would be configured when PC sends USB_REQ_SET_CONFIGURATION.
	 */
	/*
	 *  musb->context.intr_ep = os_readl(U3D_EPIER);
	 *  musb->context.ep0_csr = os_readl(U3D_EP0CSR);
	 */
#ifdef USE_SSUSB_QMU
	/*
	 * QGCSR(RXQ/TXQ Enable) and QIER0(QMU Done Interrupt Enable)
	 * would be configured at mu3d_hal_ep_enable() when PC sends USB_REQ_SET_CONFIGURATION
	 * USB_REQ_SET_CONFIGURATION -> set_config() -> f->set_alt() -> usb_ep_enable() -> musb_gadget_enable()
	 * So do _NOT_ have to save those value.
	 */
	/*
	 *  musb->context.qmu_crs = os_readl(U3D_QGCSR);
	 *  musb->context.intr_qmu_done = os_readl(U3D_QIER0);
	 */
#endif
#endif				/* CONFIG_USB_MU3D_DRV */
	for (i = 0; i < musb->config->num_eps; ++i) {
		os_printk(K_DEBUG, "%s EP%d\n", __func__, i);
#ifdef CONFIG_USB_MU3D_DRV
		/*
		 * Each TX/RX EP CSR would be configured at mu3d_hal_ep_enable() when PC sends USB_REQ_SET_CONFIGURATION
		 */
		/*
		 *   musb->context.index_regs[i].txcsr0 = USB_ReadCsr32(U3D_TX1CSR0, i+1);
		 *  musb->context.index_regs[i].txcsr1 = USB_ReadCsr32(U3D_TX1CSR1, i+1);
		 *  musb->context.index_regs[i].txcsr2 = USB_ReadCsr32(U3D_TX1CSR2, i+1);
		 *  musb->context.index_regs[i].rxcsr0 = USB_ReadCsr32(U3D_RX1CSR0, i+1);
		 *  musb->context.index_regs[i].rxcsr1 = USB_ReadCsr32(U3D_RX1CSR1, i+1);
		 *  musb->context.index_regs[i].rxcsr2 = USB_ReadCsr32(U3D_RX1CSR2, i+1);
		 */
#ifdef USE_SSUSB_QMU
		/* Save TXQ/RXQ starting address. Those would be reset to 0 after reset SSUSB IP. */
		musb->context.index_regs[i].txqmuaddr = os_readl(USB_QMU_TQSAR(i + 1));
		os_printk(K_DEBUG, "%s TQSAR[%d]=%x\n", __func__, i,
			  musb->context.index_regs[i].txqmuaddr);
		musb->context.index_regs[i].rxqmuaddr = os_readl(USB_QMU_RQSAR(i + 1));
		os_printk(K_DEBUG, "%s RQSAR[%d]=%x\n", __func__, i,
			  musb->context.index_regs[i].rxqmuaddr);
#endif
#endif				/* CONFIG_USB_MU3D_DRV */
	}
}

static void musb_restore_context(struct musb *musb)
{
	int i;

#ifdef CONFIG_USB_MU3D_DRV
	/*
	 *  os_writel(U3D_EPIESR, musb->context.intr_ep);
	 *  os_writel(U3D_EP0CSR, musb->context.ep0_csr);
	 */
#ifdef USE_SSUSB_QMU
	/*
	 *  os_writel(U3D_QGCSR, musb->context.qmu_crs);
	 *  os_writel(U3D_QIESR0, musb->context.intr_qmu_done);
	 */
#endif
#endif				/* CONFIG_USB_MU3D_DRV */

	for (i = 0; i < musb->config->num_eps; ++i) {
#ifdef CONFIG_USB_MU3D_DRV
		/*
		 *  USB_WriteCsr32(U3D_TX1CSR0, i+1, musb->context.index_regs[i].txcsr0);
		 *  USB_WriteCsr32(U3D_TX1CSR1, i+1, musb->context.index_regs[i].txcsr1);
		 *  USB_WriteCsr32(U3D_TX1CSR2, i+1, musb->context.index_regs[i].txcsr2);
		 *  USB_WriteCsr32(U3D_RX1CSR0, i+1, musb->context.index_regs[i].rxcsr0);
		 *  USB_WriteCsr32(U3D_RX1CSR1, i+1, musb->context.index_regs[i].rxcsr1);
		 *  USB_WriteCsr32(U3D_RX1CSR2, i+1, musb->context.index_regs[i].rxcsr2);
		 */
#ifdef USE_SSUSB_QMU
		os_writel(USB_QMU_TQSAR(i + 1), musb->context.index_regs[i].txqmuaddr);
		os_writel(USB_QMU_RQSAR(i + 1), musb->context.index_regs[i].rxqmuaddr);
		os_printk(K_INFO, "%s TQSAR[%d]=%x\n", __func__, i, os_readl(USB_QMU_TQSAR(i + 1)));
		os_printk(K_INFO, "%s TQSAR[%d]=%x\n", __func__, i, os_readl(USB_QMU_RQSAR(i + 1)));
#endif
#endif				/* CONFIG_USB_MU3D_DRV */
	}
}

static int musb_suspend_noirq(struct device *dev)
{
	struct musb *musb = dev_to_musb(dev);

	os_printk(K_INFO, "%s\n", __func__);
	/*
	 * Note: musb_save_context() _MUST_ be called _BEFORE_ mtu3d_suspend_noirq().
	 * Because when mtu3d_suspend_noirq() resets the SSUSB IP, All MAC regs can _NOT_ be read and be reset to
	 * the default value. So save the MUST-SAVED reg in the context structure.
	 */
	musb_save_context(musb);

	set_ssusb_ip_sleep(musb);

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* Let PHY enter savecurrent mode. And turn off CLK. */
	usb_phy_savecurrent(musb->is_clk_on);
	musb->is_clk_on = 0;
#endif

	return 0;
}

static int musb_resume_noirq(struct device *dev)
{
	struct musb *musb = dev_to_musb(dev);

	os_printk(K_INFO, "%s\n", __func__);

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* Recovert PHY. And turn on CLK. */
	usb_phy_recover(musb->is_clk_on);
	musb->is_clk_on = 1;

	/* USB 2.0 slew rate calibration */
	u3phy_ops->u2_slew_rate_calibration(u3phy);
#endif

	/* disable IP reset and power down, disable U2/U3 ip power down */
	_ex_mu3d_hal_ssusb_en();

	/* reset U3D all dev module. */
	mu3d_hal_rst_dev();

	musb_restore_context(musb);

	return 0;
}

static const struct dev_pm_ops musb_dev_pm_ops = {
	.suspend_noirq = musb_suspend_noirq,
	.resume_noirq = musb_resume_noirq,
};

#define MUSB_DEV_PM_OPS (&musb_dev_pm_ops)

#else				/* NEVER */

/* These suspend/Resume function deal with UART switch related recover only */
#ifdef CONFIG_MTK_UART_USB_SWITCH
#define MUSB_DEV_PM_OPS (&musb_dev_pm_ops)
static int musb_suspend_noirq(struct device *dev)
{
	os_printk(K_INFO, "%s: for CONFIG_MTK_UART_USB_SWITCH: in_uart_mode: %d\n", __func__,
		  in_uart_mode);

	return 0;
}

static int musb_resume_noirq(struct device *dev)
{
	os_printk(K_INFO, "%s: for CONFIG_MTK_UART_USB_SWITCH: in_uart_mode: %d\n", __func__,
		  in_uart_mode);

	if (in_uart_mode == true)
		usb_phy_switch_to_uart();

	return 0;
}

static const struct dev_pm_ops musb_dev_pm_ops = {
	.suspend_noirq = musb_suspend_noirq,
	.resume_noirq = musb_resume_noirq,
};
#else
#define	MUSB_DEV_PM_OPS	NULL
#endif				/* CONFIG_MTK_UART_USB_SWITCH */
#endif				/* NEVER */

static struct platform_driver musb_driver_probe = {
	.driver = {
		   .name = (char *)musb_driver_name,
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   .pm = MUSB_DEV_PM_OPS,
		   },
	.probe = musb_probe,
	.remove = musb_remove,
	.shutdown = musb_shutdown,
};

static int usb_test_wakelock_inited;
static struct wakeup_source usb_test_wakelock;
int mu3d_force_on;
static int set_mu3d_force_on(const char *val, const struct kernel_param *kp)
{
	int option;
	int rv;

	rv = kstrtoint(val, 10, &option);
	if (rv != 0)
		return rv;

	os_printk(K_WARNIN, "mu3d_force_on:%d, option:%d\n", mu3d_force_on, option);
	if (option == 0 || option == 1) {
		os_printk(K_WARNIN, "update to %d\n", option);
		mu3d_force_on = option;
	}

	switch (option) {
	case 2:
		os_printk(K_WARNIN, "trigger reconnect\n");
		mt_usb_connect();
		break;
	case 3:
		os_printk(K_WARNIN, "start connect test\n");
		mt_usb_connect_test(1);
		break;
	case 4:
		os_printk(K_WARNIN, "stop connect test\n");
		mt_usb_connect_test(0);
		break;
	case 5:
		os_printk(K_WARNIN, "wake_lock usb_test_wakelock\n");
		if (!usb_test_wakelock_inited) {
			os_printk(K_WARNIN, "%s wake_lock_init\n", __func__);
			wakeup_source_init(&usb_test_wakelock, "usb.test.lock");
			usb_test_wakelock_inited = 1;
		}
		__pm_stay_awake(&usb_test_wakelock);
		break;
	case 6:
		os_printk(K_WARNIN, "wake_unlock usb_test_wakelock\n");
		__pm_relax(&usb_test_wakelock);
		break;
	default:
		break;
	}
	return 0;
}
static struct kernel_param_ops mu3d_force_on_param_ops = {
	.set = set_mu3d_force_on,
	.get = param_get_int,
};
module_param_cb(mu3d_force_on, &mu3d_force_on_param_ops, &mu3d_force_on, 0644);

/*-------------------------------------------------------------------------*/
#ifdef CONFIG_USBIF_COMPLIANCE
static int musb_mu3d_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "musb_mu3d_proc_show, mu3d is %d (on:1, off:0)\n", mu3d_normal_driver_on);
	return 0;
}

static int musb_mu3d_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, musb_mu3d_proc_show, inode->i_private);
}

static ssize_t musb_mu3d_proc_write(struct file *file, const char __user *buf, size_t length,
				    loff_t *ppos)
{
	char msg[32];

	if (length >= sizeof(msg)) {
		os_printk(K_ERR, "musb_mu3d_proc_write length error, the error len is %d\n",
			  (unsigned int)length);
		return -EINVAL;
	}
	if (copy_from_user(msg, buf, length))
		return -EFAULT;

	msg[length] = 0;

	os_printk(K_DEBUG, "musb_mu3d_proc_write: %s, current driver on/off: %d\n", msg,
		  mu3d_normal_driver_on);

	if ((msg[0] == '1') && (mu3d_normal_driver_on == 0)) {
		os_printk(K_DEBUG, "registe mu3d driver ===>\n");
		init_connection_work();
		init_check_ltssm_work();
		platform_driver_register(&musb_driver_probe);
		mu3d_normal_driver_on = 1;
		Charger_Detect_En(true);
		os_printk(K_DEBUG, "registe mu3d driver <===\n");
	} else if ((msg[0] == '0') && (mu3d_normal_driver_on == 1)) {
		os_printk(K_DEBUG, "unregiste mu3d driver ===>\n");
		mu3d_normal_driver_on = 0;
		Charger_Detect_En(false);
		platform_driver_unregister(&musb_driver_probe);
		os_printk(K_DEBUG, "unregiste mu3d driver <===\n");
	} else {
		/* kernel_restart(NULL); */
		/* arch_reset(0, NULL); */
		os_printk(K_ERR, "musb_mu3d_proc_write , set reboot !\n");
		/* os_printk(K_ERR, "musb_mu3d_proc_write write faile !\n"); */
	}
	return length;
}

static const struct file_operations mu3d_proc_fops = {
	.owner = THIS_MODULE,
	.open = musb_mu3d_proc_open,
	.write = musb_mu3d_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,

};

static int __init musb_init(void)
{
	struct proc_dir_entry *prEntry;
	int ret = 0;

	if (usb_disabled())
		return 0;

	pr_info("%s: version " MUSB_VERSION ", ?dma?, otg (peripheral+host)\n", musb_driver_name);

	/* USBIF */
	prEntry = proc_create("mu3d_driver_init", 0644, NULL, &mu3d_proc_fops);

	if (prEntry)
		os_printk(K_ERR, "create the mu3d init proc OK!\n");
	else
		os_printk(K_ERR, "[ERROR] create the mu3d init proc FAIL\n");

	/* set MU3D up at boot up */
	ret = platform_driver_register(&musb_driver_probe);
	mu3d_normal_driver_on = 1;
	Charger_Detect_En(true);

	return ret;
}
module_init(musb_init);

static void __exit musb_cleanup(void)
{
	os_printk(K_ERR, "musb_cleanup\n");
	if (mu3d_normal_driver_on == 1)
		platform_driver_unregister(&musb_driver_probe);

}
module_exit(musb_cleanup);
#else

static int __init musb_init(void)
{
	if (usb_disabled())
		return 0;

	pr_info("%s: version " MUSB_VERSION ", ?dma?, otg (peripheral+host)\n", musb_driver_name);
	return platform_driver_register(&musb_driver_probe);
}
module_init(musb_init);

static void __exit musb_cleanup(void)
{
	platform_driver_unregister(&musb_driver_probe);
}
module_exit(musb_cleanup);
#endif
