/*
 * MUSB OTG peripheral driver ep0 handling
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 * Copyright (C) 2008-2009 MontaVista Software, Inc. <source@mvista.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
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

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include "musb_core.h"
#include <linux/mu3d/hal/mu3d_hal_osal.h>
#include <linux/mu3d/hal/mu3d_hal_usb_drv.h>
#include <linux/mu3d/hal/mu3d_hal_hw.h>

/* ep0 is always musb->endpoints[0].ep_in */
#define	next_ep0_request(musb)	next_in_request(&(musb)->endpoints[0])

/*
 * locking note:  we use only the controller lock, for simpler correctness.
 * It's always held with IRQs blocked.
 *
 * It protects the ep0 request queue as well as ep0_state, not just the
 * controller and indexed registers.  And that lock stays held unless it
 * needs to be dropped to allow reentering this driver ... like upcalls to
 * the gadget driver, or adjusting endpoint halt status.
 */

static char *decode_ep0stage(u8 stage)
{
	switch (stage) {
	case MUSB_EP0_STAGE_IDLE:	return "idle";
	case MUSB_EP0_STAGE_SETUP:	return "setup";
	case MUSB_EP0_STAGE_TX:		return "in";
	case MUSB_EP0_STAGE_RX:		return "out";
	case MUSB_EP0_STAGE_ACKWAIT:	return "wait";
	case MUSB_EP0_STAGE_STATUSIN:	return "in/status";
	case MUSB_EP0_STAGE_STATUSOUT:	return "out/status";
	default:			return "?";
	}
}


static int
forward_to_driver(struct musb *musb, const struct usb_ctrlrequest *ctrlrequest)
__releases(musb->lock)
__acquires(musb->lock)
{
	int usb_state = 0;
	int retval;

	os_printk(K_DEBUG, "%s\n", __func__);
	if (!musb->gadget_driver)
		return -EOPNOTSUPP;
	spin_unlock(&musb->lock);
	retval = musb->gadget_driver->setup(&musb->g, ctrlrequest);
	os_printk(K_DEBUG, "%s retval=%d\n", __func__, retval);

	if(ctrlrequest->bRequest == USB_REQ_SET_CONFIGURATION){
        if (ctrlrequest->wValue & 0xff)
	        usb_state = USB_CONFIGURED;
        else
	        usb_state = USB_UNCONFIGURED;
	    musb_sync_with_bat(musb,usb_state); /* annonce to the battery */
	}
	spin_lock(&musb->lock);
	return retval;
}


/* handle a standard GET_STATUS request
 * Context:  caller holds controller lock
 */
static int service_tx_status_request(
	struct musb *musb,
	const struct usb_ctrlrequest *ctrlrequest)
{
	int handled = 1, maxp;
	u8 result[2], epnum = 0;
	const u8 recip = ctrlrequest->bRequestType & USB_RECIP_MASK;

	os_printk(K_DEBUG, "%s\n", __func__);

	result[1] = 0;

	switch (recip) {
	case USB_RECIP_DEVICE:
		result[0] = musb->is_self_powered << USB_DEVICE_SELF_POWERED;
		result[0] |= musb->may_wakeup << USB_DEVICE_REMOTE_WAKEUP;
		//superspeed only
		if (musb->g.speed == USB_SPEED_SUPER)
		{
			result[0] |= musb->g.pwr_params.bU1Enabled << USB_DEV_STAT_U1_ENABLED;
			result[0] |= musb->g.pwr_params.bU2Enabled << USB_DEV_STAT_U2_ENABLED;
		}

		if (musb->g.is_otg) {
			result[0] |= musb->g.b_hnp_enable
				<< USB_DEVICE_B_HNP_ENABLE;
			result[0] |= musb->g.a_alt_hnp_support
				<< USB_DEVICE_A_ALT_HNP_SUPPORT;
			result[0] |= musb->g.a_hnp_support
				<< USB_DEVICE_A_HNP_SUPPORT;
		}
		os_printk(K_DEBUG, "%s result=%x, U1=%x, U2=%x\n", __func__, result[0], musb->g.pwr_params.bU1Enabled, musb->g.pwr_params.bU2Enabled);
		break;

	case USB_RECIP_INTERFACE:
		result[0] = 0;
		break;

	case USB_RECIP_ENDPOINT: {
		int		is_in;
		struct musb_ep	*ep;
		u32		tmp;

		epnum = (u8) ctrlrequest->wIndex;
		if (!epnum) {
			result[0] = 0;
			break;
		}

		is_in = epnum & USB_DIR_IN;
		if (is_in) {
			epnum &= 0x0f;
			ep = &musb->endpoints[epnum].ep_in;
		} else {
			ep = &musb->endpoints[epnum].ep_out;
		}

		if (epnum >= MUSB_C_NUM_EPS || !ep->desc) {
			handled = -EINVAL;
			break;
		}

		if (is_in)
		{
            tmp = (USB_ReadCsr32(U3D_TX1CSR0, epnum) & TX_SENDSTALL) || (USB_ReadCsr32(U3D_TX1CSR0, epnum) & TX_SENTSTALL);
            //tmp = USB_ReadCsr16(U3D_TX1CSR0, epnum) & TX_SENDSTALL;
		}
		else
		{
			tmp = (USB_ReadCsr32(U3D_RX1CSR0, epnum) & RX_SENDSTALL) || (USB_ReadCsr32(U3D_RX1CSR0, epnum) & RX_SENTSTALL);
		}

		result[0] = tmp ? 1 : 0;
		} break;

	default:
		/* class, vendor, etc ... delegate */
		handled = 0;
		break;
	}

	/* fill up the fifo; caller updates csr0 */
	if (handled > 0) {
		u16	len = le16_to_cpu(ctrlrequest->wLength);

		if (len > 2)
			len = 2;
		maxp = musb->endpoints->max_packet_sz_tx;
		mu3d_hal_write_fifo( 0,len,result, maxp);
	}

	return handled;
}

/*
 * handle a control-IN request, the end0 buffer contains the current request
 * that is supposed to be a standard control request. Assumes the fifo to
 * be at least 2 bytes long.
 *
 * @return 0 if the request was NOT HANDLED,
 * < 0 when error
 * > 0 when the request is processed
 *
 * Context:  caller holds controller lock
 */
static int
service_in_request(struct musb *musb, const struct usb_ctrlrequest *ctrlrequest)
{
	int handled = 0;	/* not handled */

	os_printk(K_DEBUG, "%s\n", __func__);

	if ((ctrlrequest->bRequestType & USB_TYPE_MASK)
			== USB_TYPE_STANDARD) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_GET_STATUS:
			os_printk(K_DEBUG, "USB_REQ_GET_STATUS\n");
			handled = service_tx_status_request(musb,
					ctrlrequest);
			break;

		/* case USB_REQ_SYNC_FRAME: */

		default:
			break;
		}
	}
	return handled;
}

/*
 * Context:  caller holds controller lock
 */
static void musb_g_ep0_giveback(struct musb *musb, struct usb_request *req)
{
	os_printk(K_DEBUG, "%s\n", __func__);

	musb_g_giveback(&musb->endpoints[0].ep_in, req, 0);
}

/*
 * Tries to start B-device HNP negotiation if enabled via sysfs
 */
static inline void musb_try_b_hnp_enable(struct musb *musb)
{
	u32		devctl;

	dev_dbg(musb->controller, "HNP: Setting HR\n");
	devctl = os_readl(U3D_DEVICE_CONTROL);
	//os_writeb(mbase + MAC_DEVICE_CONTROL, devctl | USB_DEVCTL_HOSTREQUEST); //We temporarily disable this because the bitfile doesn't support yet.
}

/*
 * Handle all control requests with no DATA stage, including standard
 * requests such as:
 * USB_REQ_SET_CONFIGURATION, USB_REQ_SET_INTERFACE, unrecognized
 *	always delegated to the gadget driver
 * USB_REQ_SET_ADDRESS, USB_REQ_CLEAR_FEATURE, USB_REQ_SET_FEATURE
 *	always handled here, except for class/vendor/... features
 *
 * Context:  caller holds controller lock
 */
static int
service_zero_data_request(struct musb *musb,
		struct usb_ctrlrequest *ctrlrequest)
__releases(musb->lock)
__acquires(musb->lock)
{
	int handled = -EINVAL;
	const u8 recip = ctrlrequest->bRequestType & USB_RECIP_MASK;
#ifdef CONFIG_USBIF_COMPLIANCE
	unsigned int tmp;
#endif

	os_printk(K_DEBUG, "%s\n", __func__);

	/* the gadget driver handles everything except what we MUST handle */
	if ((ctrlrequest->bRequestType & USB_TYPE_MASK)
			== USB_TYPE_STANDARD) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_SET_ISOCH_DELAY:
			handled = 1;
			break;
		case USB_REQ_SET_ADDRESS:
			/* change it after the status stage */
			musb->set_address = true;
			musb->address = (u8) (ctrlrequest->wValue & 0x7f);
			handled = 1;
			break;

		case USB_REQ_CLEAR_FEATURE:
			switch (recip) {
			case USB_RECIP_DEVICE:
				switch (ctrlrequest->wValue)
				{
					case USB_DEVICE_REMOTE_WAKEUP:
						musb->may_wakeup = 0;
						handled = 1;
						break;
					case USB_DEVICE_U1_ENABLE:
					case USB_DEVICE_U2_ENABLE:
						//superspeed only
						if (musb->g.speed == USB_SPEED_SUPER) {
							//forward the request because of device state check
							handled = forward_to_driver(musb, ctrlrequest);

#ifdef CONFIG_USBIF_COMPLIANCE
							if (handled >= 0) {
								tmp = (ctrlrequest->wValue == USB_DEVICE_U1_ENABLE) ?
								SW_U1_ACCEPT_ENABLE : SW_U2_ACCEPT_ENABLE;

								os_printk(K_INFO, "%s CLEAR_FEATURE reg=%x, tmp=%x\n",
									__func__, os_readl(U3D_LINK_POWER_CONTROL), tmp);

								os_clrmsk(U3D_LINK_POWER_CONTROL, tmp);

								os_printk(K_INFO, "%s CLEAR_FEATURE reg=%x\n",
									__func__, os_readl(U3D_LINK_POWER_CONTROL));

								if (ctrlrequest->wValue == USB_DEVICE_U1_ENABLE)
									musb->g.pwr_params.bU1Enabled = 0;
								else
									musb->g.pwr_params.bU2Enabled = 0;
							} else {
								os_printk(K_ERR, "[COM] composite driver can not handle this control request!\n");
							}
#else
							/*
							 * DO NOT SUPPORT U1/U2 for IOT. Return STALL.
							 * Under TUSB7340 + Lenovo MT-M5852-B88, IOT test failed.
							 */
							handled = -EINVAL;

#endif /* NEVER */
						}
						break;
					default:
						handled = -EINVAL;
						break;
				}
				break;
			case USB_RECIP_INTERFACE:
				//superspeed only
				if ((ctrlrequest->wValue == USB_INTRF_FUNC_SUSPEND)
					&& (musb->g.speed == USB_SPEED_SUPER))
					//forward the request because of device state check
					handled = forward_to_driver(musb, ctrlrequest);
				break;
			case USB_RECIP_ENDPOINT:{
				const u8		epnum =
					ctrlrequest->wIndex & 0x0f;
				struct musb_ep		*musb_ep;
				struct musb_hw_ep	*ep;
				struct musb_request	*request;
				int			is_in;
				u32			csr;

				if (epnum == 0 || epnum >= MUSB_C_NUM_EPS ||
				    ctrlrequest->wValue != USB_ENDPOINT_HALT)
					break;

				ep = musb->endpoints + epnum;
				is_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (is_in)
					musb_ep = &ep->ep_in;
				else
					musb_ep = &ep->ep_out;
				if (!musb_ep->desc)
					break;

				handled = 1;
				/* Ignore request if endpoint is wedged */
				if (musb_ep->wedged)
					break;

				if (is_in) {//TX
					csr = USB_ReadCsr32(U3D_TX1CSR0, epnum) & TX_W1C_BITS;
					csr = (csr & (~TX_SENDSTALL)) | TX_SENTSTALL;
					//csr = csr & (~TX_SENDSTALL);

					USB_WriteCsr32(U3D_TX1CSR0, epnum, csr);

					os_printk(K_DEBUG, "&&&&&&&&&&&&&&&&&&&&&&&&&& clear tx stall --> write csr[%d] 0x%04x. new CSR is: 0x%04x\n", epnum, csr, USB_ReadCsr32(U3D_TX1CSR0, epnum));

					os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) | (BIT16<<epnum));//reset TX EP
					os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) & ~(BIT16<<epnum));//reset reset TX EP
					os_printk(K_DEBUG, "RST TX%d\n", epnum);

					/* We cannot flush QMU now, because the MSC gadget will not re-submit the CBW request after clear halt. */

					//_ex_mu3d_hal_flush_qmu(epnum, USB_TX);
					//mu3d_hal_restart_qmu(epnum, USB_TX);

				} else {
					csr = USB_ReadCsr32(U3D_RX1CSR0, epnum) & RX_W1C_BITS;
					csr = (csr & (~RX_SENDSTALL)) | RX_SENTSTALL;

					USB_WriteCsr32(U3D_RX1CSR0, epnum, csr);

					os_printk(K_DEBUG, "&&&&&&&&&&&&&&&&&&&&&&&&&& clear rx stall --> write csr[%d] 0x%04x. new CSR is: 0x%04x\n", epnum, csr, USB_ReadCsr32(U3D_RX1CSR0, epnum) );

					os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) | (1 << epnum));//reset RX EP
					os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) & (~(1 << epnum)));//reset reset RX EP
					os_printk(K_DEBUG, "RST RX%d\n", epnum);
					/* We cannot flush QMU now, because the MSC gadget will not re-submit the CBW request after clear halt. */

					//_ex_mu3d_hal_flush_qmu(epnum, USB_RX);
					//mu3d_hal_restart_qmu(epnum, USB_RX);
				}

				/* Maybe start the first request in the queue */
				request = next_request(musb_ep);
				if (!musb_ep->busy && request) {
					dev_dbg(musb->controller, "restarting the request\n");
					musb_ep_restart(musb, request);
				}

				/* select ep0 again */
				} break;
			default:
				/* class, vendor, etc ... delegate */
				handled = 0;
				break;
			}
			break;

		case USB_REQ_SET_FEATURE:
			switch (recip) {
			case USB_RECIP_DEVICE:
				handled = 1;
				switch (ctrlrequest->wValue) {
				case USB_DEVICE_REMOTE_WAKEUP:
					musb->may_wakeup = 1;
					break;
				case USB_DEVICE_TEST_MODE:
					if (musb->g.speed != USB_SPEED_HIGH)
						goto stall;
					if (ctrlrequest->wIndex & 0xff)
						goto stall;

					switch (ctrlrequest->wIndex >> 8) {
					case 1:
						pr_debug("TEST_J\n");
						/* TEST_J */
						musb->test_mode_nr =
							TEST_J_MODE;
						break;
					case 2:
						/* TEST_K */
						pr_debug("TEST_K\n");
						musb->test_mode_nr =
							TEST_K_MODE;
						break;
					case 3:
						/* TEST_SE0_NAK */
						pr_debug("TEST_SE0_NAK\n");
						musb->test_mode_nr =
							TEST_SE0_NAK_MODE;
						break;
					case 4:
						/* TEST_PACKET */
						pr_debug("TEST_PACKET\n");
						musb->test_mode_nr =
							TEST_PACKET_MODE;
						break;

					case 0xc0:
						/* TEST_FORCE_HS */
						pr_debug("TEST_FORCE_HS\n");
						musb->test_mode_nr =
							FORCE_HS;
						break;
					case 0xc1:
						/* TEST_FORCE_FS */
						pr_debug("TEST_FORCE_FS\n");
						musb->test_mode_nr =
							FORCE_FS;
						break;
					case 0xc2:
						/* TEST_FIFO_ACCESS */
						pr_debug("TEST_FIFO_ACCESS\n");
						musb->test_mode_nr =
							FIFO_ACCESS;
						break;
					case 0xc3:
						/* TEST_FORCE_HOST */
						pr_debug("TEST_FORCE_HOST\n");
						musb->test_mode_nr =
							FORCE_HOST;
						break;
					default:
						goto stall;
					}

					/* enter test mode after irq */
					if (handled > 0)
						musb->test_mode = true;
					break;
				case USB_DEVICE_B_HNP_ENABLE:
					if (!musb->g.is_otg)
						goto stall;
					musb->g.b_hnp_enable = 1;
					musb_try_b_hnp_enable(musb);
					break;
				case USB_DEVICE_A_HNP_SUPPORT:
					if (!musb->g.is_otg)
						goto stall;
					musb->g.a_hnp_support = 1;
					break;
				case USB_DEVICE_A_ALT_HNP_SUPPORT:
					if (!musb->g.is_otg)
						goto stall;
					musb->g.a_alt_hnp_support = 1;
					break;
				case USB_DEVICE_U1_ENABLE:
				case USB_DEVICE_U2_ENABLE:
					//superspeed only
					if (musb->g.speed == USB_SPEED_SUPER)
					{
						//forward the request because of device state check
						handled = forward_to_driver(musb, ctrlrequest);

#ifdef CONFIG_USBIF_COMPLIANCE
						if (handled >= 0) {

							tmp = (ctrlrequest->wValue == USB_DEVICE_U1_ENABLE) ?
								(SW_U1_ACCEPT_ENABLE) :
								(SW_U2_ACCEPT_ENABLE);

							os_printk(K_INFO, "%s SET_FEATURE %s handled=%d val=%x\n",
								__func__,
								(ctrlrequest->wValue == USB_DEVICE_U1_ENABLE)?"U1":"U2",
								handled, tmp);

							os_setmsk(U3D_LINK_POWER_CONTROL, tmp);

							os_printk(K_INFO, "%s SET_FEATURE reg=%x\n",
  								__func__, os_readl(U3D_LINK_POWER_CONTROL));

							if (ctrlrequest->wValue == USB_DEVICE_U1_ENABLE)
								musb->g.pwr_params.bU1Enabled = 1;
							else
								musb->g.pwr_params.bU2Enabled = 1;
						} else {
							os_printk(K_ERR, "[COM] composite driver can not handle this control request!\n");
						}
#else
						/*
						 * DO NOT SUPPORT U1/U2 for IOT. Return STALL.
						 * Under TUSB7340 + Lenovo MT-M5852-B88, IOT test failed.
						 */
						handled = -EINVAL;

#endif /* NEVER */
					}
					break;
				case USB_DEVICE_DEBUG_MODE:
					handled = 0;
					break;
stall:
				default:
					handled = -EINVAL;
					break;
				}
				break;

			case USB_RECIP_INTERFACE:
				//superspeed only
				if ((ctrlrequest->wValue == USB_INTRF_FUNC_SUSPEND)
					&& (musb->g.speed == USB_SPEED_SUPER)) {
					//forward the request because of device state check
					//handled = forward_to_driver(musb, ctrlrequest);

					os_printk(K_DEBUG, "wIndex=%x\n", ctrlrequest->wIndex);

					if(ctrlrequest->wIndex & USB_INTRF_FUNC_SUSPEND_LP)
						os_printk(K_DEBUG, "USB_INTRF_FUNC_SUSPEND_LP\n");
					else if(ctrlrequest->wIndex & USB_INTRF_FUNC_SUSPEND_RW)
						os_printk(K_DEBUG, "USB_INTRF_FUNC_SUSPEND_RW\n");
				}
				os_printk(K_DEBUG, "USB_RECIP_INTERFACE handled=%d\n", handled);

				/* Just pretend that the gadget driver can fully handle this control request*/
				handled = 1;

				break;

			case USB_RECIP_ENDPOINT:{
				const u8		epnum =
					ctrlrequest->wIndex & 0x0f;
				struct musb_ep		*musb_ep;
				struct musb_hw_ep	*ep;
				int			is_in;
				u32			csr;

				if (epnum == 0 || epnum >= MUSB_C_NUM_EPS ||
				    ctrlrequest->wValue	!= USB_ENDPOINT_HALT)
					break;

				ep = musb->endpoints + epnum;
				is_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (is_in)
					musb_ep = &ep->ep_in;
				else
					musb_ep = &ep->ep_out;
				if (!musb_ep->desc)
					break;

				if (is_in) {//tx
					csr = USB_ReadCsr32(U3D_TX1CSR0, epnum);

					if (!(csr & TX_FIFOEMPTY))
					{
						/*

						csr &= TX_W1C_BITS; //don't clear W1C bits
						csr |= USB_TXCSR_FLUSHFIFO;
						//os_printk(K_DEBUG, "EP%d USB_TXCSR_FLUSHFIFO\n", epnum);
						SSUSB_WriteCsr16(U3D_TX1CSR0, epnum, csr ); //flush fifo before sendstall. Follow ssusb programming guide.
						while(USB_ReadCsr16(U3D_TX1CSR0, epnum) & USB_TXCSR_FLUSHFIFO)
						{
							cpu_relax();
						}

						*/

						os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) | (BIT16<<epnum));//reset TX EP
						os_writel(U3D_EP_RST, os_readl(U3D_EP_RST) & ~(BIT16<<epnum));//reset reset TX EP

					}


					csr &= TX_W1C_BITS;
					csr |= TX_SENDSTALL;
					os_printk(K_DEBUG, "@@@@@@@@@@@@@@@@@ EP%d(IN/TX) SEND_STALL\n", epnum);

					//ssusb: need further check. is WZC_BITS needed?

					USB_WriteCsr32(U3D_TX1CSR0, epnum, csr);
				} else {
					csr = USB_ReadCsr32(U3D_RX1CSR0, epnum);
					csr &= RX_W1C_BITS;
					csr |= RX_SENDSTALL;

					os_printk(K_DEBUG, "@@@@@@@@@@@@@@@@@ EP%d(OUT/RX) SEND_STALL\n", epnum);
					//musb_writew(regs, MUSB_RXCSR, csr);
					USB_WriteCsr32(U3D_RX1CSR0, epnum, csr);
				}

				/* select ep0 again */
				handled = 1;
				} break;

			default:
				/* class, vendor, etc ... delegate */
				handled = 0;
				break;
			}
			break;
		default:
			/* delegate SET_CONFIGURATION, etc */
			handled = 0;
		}
	} else
		handled = 0;
	return handled;
}

/* we have an ep0out data packet
 * Context:  caller holds controller lock
 */
static void ep0_rxstate(struct musb *musb)
{
	struct musb_request	*request;
	struct usb_request	*req;
	u32 csr;
	u16 count = 0;

	os_printk(K_DEBUG, "%s\n", __func__);

	/* Set the register which is W1C as 0. */
	csr = os_readl(U3D_EP0CSR) & EP0_W1C_BITS;

	request = next_ep0_request(musb);
	req = &request->request;

	/* read packet and ack; or stall because of gadget driver bug:
	 * should have provided the rx buffer before setup() returned.
	 */
	if (req) {
		void *buf = req->buf + req->actual;
		unsigned	len = req->length - req->actual;

#ifdef AUTOCLEAR
		if(!(os_readl(U3D_EP0CSR) & EP0_AUTOCLEAR)){
			os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)|EP0_AUTOCLEAR);
		}
#endif
		/* read the buffer */
		count = os_readl(U3D_RXCOUNT0);
		if (count > len) {
			req->status = -EOVERFLOW;
			count = len;
		}
		musb_read_fifo(&musb->endpoints[0], count, buf);
		req->actual += count;
		csr |= EP0_RXPKTRDY;
		/*REVISIT-J: 64 is usb20 ep0 maxp, but usb30 ep0 maxp is 512. Do we need the modification?*/
		if (count < 64 || req->actual == req->length) {
			//musb->ep0_state = MUSB_EP0_STAGE_STATUSIN;
			musb->ep0_state = MUSB_EP0_IDLE; //in ssusb, there is no interrupt to transit to idle phase.
			os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_STATUSIN then MUSB_EP0_IDLE\n");

			csr |= EP0_DATAEND;
		} else
			req = NULL;
	} else {
		csr |= EP0_RXPKTRDY | EP0_SENDSTALL;
		os_printk(K_DEBUG, "@@@@@@@@@@@@@@@ SENDSTALL\n");
	}

	/* Completion handler may choose to stall, e.g. because the
	 * message just received holds invalid data.
	 */
	if (req) {
		musb->ackpend = csr;
		musb_g_ep0_giveback(musb, req);
		if (!musb->ackpend)
			return;
		musb->ackpend = 0;
	}
	os_writel(U3D_EP0CSR, csr);
}

/*
 * transmitting to the host (IN), this code might be called from IRQ
 * and from kernel thread.
 *
 * Context:  caller holds controller lock
 */
static void ep0_txstate(struct musb *musb)
{
	struct musb_request	*req = next_ep0_request(musb);
	struct usb_request	*request;

	u32			csr = EP0_TXPKTRDY;
	u8			*fifo_src;
	u16			fifo_count;
	u32 		maxp;

	os_printk(K_DEBUG, "%s\n", __func__);

	if (!req) {
		/* WARN_ON(1); */
		//dev_dbg(musb->controller, "odd; csr0 %04x\n", musb_readw(regs, MUSB_CSR0));
		return;
	}

	maxp = musb->endpoints->max_packet_sz_tx;

	request = &req->request;

	/* load the data */
	fifo_src = (u8 *) request->buf + request->actual;
	fifo_count = min((unsigned) maxp,
		request->length - request->actual);
	musb_write_fifo(&musb->endpoints[0], fifo_count, fifo_src);

	os_printk(K_DEBUG, "%s act=%d, len=%d, cnt=%d, maxp=%d zero=%d\n", \
		__func__, request->actual, request->length, fifo_count, maxp, request->zero);

	/*
	 * The flow is difference between MTU3D and original musb.
	 * For example:
	 * Host <-- 12byte -- Device
	 * MUSB:
	 *  Interrupt #1 : Write 12bytes in FIFO and set TXPKTRDY + DATAEND
	 *  Interrupt #2 : Do nothing.
	 * MTU3D:
	 *  Interrupt #1 : Write 12bytes in FIFO and set TXPKTRDY
	 *  Interrupt #2 : set DATAEND
	 */
	/* update the flags */
	if(request->actual == request->length) {
		if (request->zero && (request->length % maxp == 0) && (request->length / maxp > 0)) {
			/*
			 * Send a ZLP without DATAEND. Because the length host requires
			 * is longer than the actual data device sends. So pad a ZLP to
			 * tell HOST the end of the data. If the transfered data length
			 * is exactly what host wants, just set DATAEND without ZLP.
			 */
			request->zero = 0;
			os_printk(K_DEBUG, "%s Send a padding ZLP!!!!\n", __func__);
			request = NULL;
		} else {
			musb->ep0_state = MUSB_EP0_STAGE_STATUSOUT;
			csr |= EP0_DATAEND;
		}
	} else {
		/* Add the amount of the data written into fifo to "actual" first.*/
		request->actual += fifo_count;
		request = NULL;
	}

	/* report completions as soon as the fifo's loaded; there's no
	 * win in waiting till this last packet gets acked.  (other than
	 * very precise fault reporting, needed by USB TMC; possible with
	 * this hardware, but not usable from portable gadget drivers.)
	 */
	if (request) {
		musb->ackpend = csr;
		musb_g_ep0_giveback(musb, request);
		if (!musb->ackpend)
			return;
		musb->ackpend = 0;
	}

	os_printk(K_DEBUG, "%s csr=%x\n", __func__, csr);

	/* send it out, triggering a "txpktrdy cleared" irq */
	os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR) | csr);
}

/*
 * Read a SETUP packet (struct usb_ctrlrequest) from the hardware.
 * Fields are left in USB byte-order.
 *
 * Context:  caller holds controller lock.
 */
static void
musb_read_setup(struct musb *musb, struct usb_ctrlrequest *req)
{
	struct musb_request	*r;
	u32 csr = 0;

	os_printk(K_DEBUG, "%s\n", __func__);

	csr = os_readl(U3D_EP0CSR) & EP0_W1C_BITS; //Don't W1C
	if(!(os_readl(U3D_EP0CSR) & EP0_AUTOCLEAR)){
		os_writel(U3D_EP0CSR, os_readl(U3D_EP0CSR)|EP0_AUTOCLEAR);
	}
	mu3d_hal_read_fifo(0,(u8 *)req);

	/* NOTE:  earlier 2.6 versions changed setup packets to host
	 * order, but now USB packets always stay in USB byte order.
	 */
	//dev_dbg(musb->controller, "SETUP req%02x.%02x v%04x i%04x l%d\n",
	os_printk(K_DEBUG, "SETUP req%02x.%02x v%04x i%04x l%d\n",
		req->bRequestType,
		req->bRequest,
		le16_to_cpu(req->wValue),
		le16_to_cpu(req->wIndex),
		le16_to_cpu(req->wLength));

	/* clean up any leftover transfers */
	r = next_ep0_request(musb);
	if (r)
		musb_g_ep0_giveback(musb, &r->request);

	/* For zero-data requests we want to delay the STATUS stage to
	 * avoid SETUPEND errors.  If we read data (OUT), delay accepting
	 * packets until there's a buffer to store them in.
	 *
	 * If we write data, the controller acts happier if we enable
	 * the TX FIFO right away, and give the controller a moment
	 * to switch modes...
	 */
	musb->set_address = false;
	if (req->wLength == 0) {
		musb->ep0_state = MUSB_EP0_STAGE_ACKWAIT;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_ACKWAIT\n");
	} else if (req->bRequestType & USB_DIR_IN) {
		musb->ep0_state = MUSB_EP0_STAGE_TX;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_TX\n");
		os_writel(U3D_EP0CSR, csr | EP0_SETUPPKTRDY | EP0_DPHTX);
		musb->ackpend = 0;
	} else {
		os_writel(U3D_EP0CSR, (csr | EP0_SETUPPKTRDY) & (~EP0_DPHTX)); //Set CSR0.SetupPktRdy(W1C) & CSR0.DPHTX=0
		musb->ackpend = 0;
		musb->ep0_state = MUSB_EP0_STAGE_RX;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_RX\n");
	}
}


/*
 * Handle peripheral ep0 interrupt
 *
 * Context: irq handler; we won't re-enter the driver that way.
 */
irqreturn_t musb_g_ep0_irq(struct musb *musb)
{
	u32		csr;
	u16		len;
	irqreturn_t	retval = IRQ_NONE;

	csr = os_readl(U3D_EP0CSR);
	len = (u16)os_readl(U3D_RXCOUNT0);

	os_printk(K_DEBUG, "%s csr=0x%X\n", __func__, csr);

	//dev_dbg(musb->controller, "csr %04x, count %d, myaddr %d, ep0stage %s\n",
	//		csr, len,
	//		musb_readb(mbase, MUSB_FADDR),
	//		decode_ep0stage(musb->ep0_state));

	//if (csr & MUSB_CSR0_P_DATAEND) {
	//	/*
	//	 * If DATAEND is set we should not call the callback,
	//	 * hence the status stage is not complete.
	//	 */
	//	return IRQ_HANDLED;
	//}

	/* I sent a stall.. need to acknowledge it now.. */
	if (csr & EP0_SENTSTALL) {
		os_writel(U3D_EP0CSR, (csr & EP0_W1C_BITS) | EP0_SENTSTALL); //EP0_SENTSTALL is W1C

		if (os_readl(U3D_EP0CSR) & EP0_TXPKTRDY) //try to flushfifo after clear sentstall
		{
			#if 0
			u32 csr0 = 0;

			csr0 = os_readl(U3D_EP0CSR);
			csr0 &= EP0_W1C_BITS; //don't clear W1C bits
			csr0 |= CSR0_FLUSHFIFO;
			os_writel(U3D_EP0CSR, csr0);

			os_printk(K_DEBUG, "waiting for flushfifo.....");
			while(os_readl(U3D_EP0CSR) & CSR0_FLUSHFIFO) //proceed before it clears
			{
				cpu_relax();
			}
			os_printk(K_DEBUG, "done.\n");
			#else
			//toggle EP0_RST
			os_setmsk(U3D_EP_RST, EP0_RST);
			os_clrmsk(U3D_EP_RST, EP0_RST);
			#endif
		}
		retval = IRQ_HANDLED;
		musb->ep0_state = MUSB_EP0_STAGE_IDLE;
		csr = os_readl(U3D_EP0CSR);
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE. now csr is 0x%04x\n", csr);
	}

/* SSUSB does not support this bit. So comment it.*/
#ifdef NEVER
	/* request ended "early" */
	if (csr & CSR0_SETUPEND) {
		os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | CSR0_SERVICESETUPEND);
		retval = IRQ_HANDLED;
		/* Transition into the early status phase */
		switch (musb->ep0_state) {
		case MUSB_EP0_STAGE_TX:
			musb->ep0_state = MUSB_EP0_STAGE_STATUSOUT;
			os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_STATUSOUT\n");
			break;
		case MUSB_EP0_STAGE_RX:
			musb->ep0_state = MUSB_EP0_STAGE_STATUSIN;
			os_printk(K_DEBUG, 	"----- ep0 state: MUSB_EP0_STAGE_STATUSIN\n");
			break;
		default:
			ERR("SetupEnd came in a wrong ep0stage %s\n",
			decode_ep0stage(musb->ep0_state));
		}
		csr = os_readl(U3D_EP0CSR);
		/* NOTE:  request may need completion */
	}
#endif /* NEVER */

	os_printk(K_DEBUG, "ep0_state=%d\n", musb->ep0_state);

	/* docs from Mentor only describe tx, rx, and idle/setup states.
	 * we need to handle nuances around status stages, and also the
	 * case where status and setup stages come back-to-back ...
	 */
	switch (musb->ep0_state) {

	case MUSB_EP0_STAGE_TX:
		/* irq on clearing txpktrdy */
		if ((csr & EP0_FIFOFULL) == 0) {
			os_printk(K_DEBUG, "csr & EP0_FIFOFULL\n");
			ep0_txstate(musb);
			retval = IRQ_HANDLED;
		}
		break;

	case MUSB_EP0_STAGE_RX:
		/* irq on set rxpktrdy */
		if (csr & EP0_RXPKTRDY) {
			ep0_rxstate(musb);
			retval = IRQ_HANDLED;
		}
		break;

	case MUSB_EP0_STAGE_STATUSIN:
/* Because ssusb doesn't have interrupt 	after In status, we actually don't have STATUSIN stage. It has been moved to MUSB_EP0_STAGE_SETUP.		*/
#if 0
		/* end of sequence #2 (OUT/RX state) or #3 (no data) */

		/* update address (if needed) only @ the end of the
		 * status phase per usb spec, which also guarantees
		 * we get 10 msec to receive this irq... until this
		 * is done we won't see the next packet.
		 */
		if (musb->set_address) {
			musb->set_address = false;
			os_writel(U3D_DEVICE_CONF, os_readl(U3D_DEVICE_CONF) | (musb->address << DEV_ADDR_OFST));
		}

		/* enter test mode if needed (exit by reset) */
		else if (musb->test_mode) {
			dev_dbg(musb->controller, "entering TESTMODE\n");

			printk("entering TESTMODE 1\n");

			if (TEST_PACKET_MODE == musb->test_mode_nr)
				musb_load_testpacket(musb);

			os_writel(U3D_USB2_TEST_MODE, musb->test_mode_nr);
		}
#endif
		/* FALLTHROUGH */

	case MUSB_EP0_STAGE_STATUSOUT:
		/* end of sequence #1: write to host (TX state) */
		{
			struct musb_request	*req;

			req = next_ep0_request(musb);
			if (req)
				musb_g_ep0_giveback(musb, &req->request);
		}

		/*
		 * In case when several interrupts can get coalesced,
		 * check to see if we've already received a SETUP packet...
		 */
		if (csr & EP0_SETUPPKTRDY) //in ssusb, we check SETUPPKTRDY for setup packet.
			goto setup;

		retval = IRQ_HANDLED;
		musb->ep0_state = MUSB_EP0_STAGE_IDLE;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE\n");
		break;

	case MUSB_EP0_STAGE_IDLE:
		/*
		 * This state is typically (but not always) indiscernible
		 * from the status states since the corresponding interrupts
		 * tend to happen within too little period of time (with only
		 * a zero-length packet in between) and so get coalesced...
		 */
		retval = IRQ_HANDLED;

		/* REVISIT-J: No need, Cuz the following sequence does not effect.*/
		// added for ssusb:
		if (!(csr & EP0_SETUPPKTRDY))
		{
		    os_printk(K_DEBUG, "break from MUSB_EP0_STAGE_IDLE\n");
			break; //Don't process, keep idle. a.
		}
		//added for ssusb.

		musb->ep0_state = MUSB_EP0_STAGE_SETUP;
                os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_SETUP\n");
		/* FALLTHROUGH */

	case MUSB_EP0_STAGE_SETUP:
setup:
		if (csr & EP0_SETUPPKTRDY) {
			struct usb_ctrlrequest	setup;
			int			handled = 0;

			if (len != 8) {
				ERR("SETUP packet len %d != 8 ?\n", len);
				break;
			}
			musb_read_setup(musb, &setup);
			retval = IRQ_HANDLED;

			/* sometimes the RESET won't be reported */
			if (unlikely(musb->g.speed == USB_SPEED_UNKNOWN)) {
				/*REVISIT-J: Shall we implement it?*/
/* mark temporarily for ssusb because PMU is not ready.
				u8	power;

				printk(KERN_NOTICE "%s: peripheral reset "
						"irq lost!\n",
						musb_driver_name);
				power = musb_readb(mbase, MUSB_POWER);
				musb->g.speed = (power & HS_MODE)
					? USB_SPEED_HIGH : USB_SPEED_FULL;

*/
			}

			switch (musb->ep0_state) {

			/* sequence #3 (no data stage), includes requests
			 * we can't forward (notably SET_ADDRESS and the
			 * device/endpoint feature set/clear operations)
			 * plus SET_CONFIGURATION and others we must
			 */
			case MUSB_EP0_STAGE_ACKWAIT:
				os_printk(K_DEBUG, "&&&&&&&&&&&&&&&&& process MUSB_EP0_STAGE_ACKWAIT\n");
				handled = service_zero_data_request(
						musb, &setup);

				/*
				 * We're expecting no data in any case, so
				 * always set the DATAEND bit -- doing this
				 * here helps avoid SetupEnd interrupt coming
				 * in the idle stage when we're stalling...
				 */
				if(1) //Because status phase currently doesn't have interrupt, we process here.
				{
					//Process here according to ssusb programming guide
					if (musb->set_address) {
						musb->set_address = false;
						//musb_writeb(mbase, MUSB_FADDR, musb->address);
						os_printk(K_INFO, "Set address to 0x%08x...\n", musb->address);
						os_writel(U3D_DEVICE_CONF, os_readl(U3D_DEVICE_CONF) | (musb->address << DEV_ADDR_OFST));
					}
					else if (musb->test_mode) {
						os_printk(K_DEBUG, "entering TESTMODE 2\n");

						if (TEST_PACKET_MODE == musb->test_mode_nr)
							musb_load_testpacket(musb);

						//musb_writeb(mbase, MUSB_TESTMODE,
						//		musb->test_mode_nr);

						//Need to send status before really entering test mode.
						os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | musb->ackpend | EP0_DATAEND | EP0_SETUPPKTRDY);
						musb->ep0_state = MUSB_EP0_STAGE_IDLE;

						while((os_readl(U3D_EP0CSR) & EP0_DATAEND) != 0)//Need to wait for status really loaded by host
						{
							mdelay(1); //Without this delay, it will fail.
						}

						os_writel(U3D_USB2_TEST_MODE, musb->test_mode_nr);

						return retval;
					}

					/* end of sequence #1: write to host (TX state) */
					{
						struct usb_request	*request;
						struct musb_request *req;

						req = next_ep0_request(musb);
						if (req)
						{
						    request = &(req->request);
							os_printk(K_DEBUG, "&&&&&&&&&&&&&&&&& next_ep0_request\n");
							musb_g_ep0_giveback(musb, request);
						}
						else
						{
							//os_printk(K_DEBUG, "&&&&&&&&&&&&&&&&& next_ep0_request returns null\n");
						}
					}
				}

                musb->ackpend |= EP0_DATAEND | EP0_SETUPPKTRDY; //Set CSR0.SetupPktRdy(W1C) & CSR0.DataEnd

				/* status stage might be immediate */
				if (handled > 0)
				{
					musb->ep0_state = MUSB_EP0_STAGE_IDLE; //Change to idle because status in will be completed immediately after dataend is set
					os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE\n");
				}

				break;

			/* sequence #1 (IN to host), includes GET_STATUS
			 * requests that we can't forward, GET_DESCRIPTOR
			 * and others that we must
			 */
			case MUSB_EP0_STAGE_TX:
				handled = service_in_request(musb, &setup);
				if (handled > 0) {
					int i = 0;
					os_printk(K_DEBUG, "handled MUSB_EP0_STAGE_TX.\n");

					/* Wait until FIFOFULL cleared by hrdc */
					while((os_readl(U3D_EP0CSR) & EP0_FIFOFULL)) {
						mdelay(5);
						i++;
						if (i > 5) {
							os_printk(K_INFO, "ep0 still full, something wrong!!!\n");
							break;
						}
					}

					musb->ackpend |= EP0_DATAEND;
					musb->ep0_state = MUSB_EP0_STAGE_STATUSOUT;
					os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_STATUSOUT (%s:%d)\n", __func__, __LINE__ );

					if(1){ //process MUSB_EP0_STAGE_STATUSOUT because currently we don't have interrupt after status out phase.
						/* end of sequence #1: write to host (TX state) */
						{
							struct usb_request	*request;
							struct musb_request *req;

							req = next_ep0_request(musb);
							if (req){
								request = &(req->request);
								musb_g_ep0_giveback(musb, request);
							}
						}

						/*
						 * In case when several interrupts can get coalesced,
						 * check to see if we've already received a SETUP packet...
						 */

						retval = IRQ_HANDLED;
						musb->ep0_state = MUSB_EP0_STAGE_IDLE;
						os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE (%s:%d)\n", __func__, __LINE__ );
					}	 //process MUSB_EP0_STAGE_STATUSOUT because currently we don't have interrupt after status out phase.

				}
				else
				{
					//os_printk(K_DEBUG, "Cannot service_in_request.\n");
				}
				break;

			/* sequence #2 (OUT from host), always forward */
			default:		/* MUSB_EP0_STAGE_RX */
				break;
			}

			//dev_dbg(musb->controller, "handled %d, csr %04x, ep0stage %s\n",
			os_printk(K_DEBUG, "handled %d, csr %04x, ep0stage %s\n",
				handled, csr,
				decode_ep0stage(musb->ep0_state));

			/* unless we need to delegate this to the gadget
			 * driver, we know how to wrap this up:  csr0 has
			 * not yet been written.
			 */
			if (handled < 0)
				goto stall;
			else if (handled > 0)
				goto finish;

			handled = forward_to_driver(musb, &setup);
			if (handled < 0) {
stall:
				os_printk(K_INFO, "stall (%d)\n", handled);
				//flushfifo before send SENDSTALL

				if (os_readl(U3D_EP0CSR) & EP0_TXPKTRDY) //try to flushfifo after clear sentstall
				{
					#if 0
					u32 csr0 = 0;

					csr0 = os_readl(U3D_EP0CSR);
					csr0 &= EP0_W1C_BITS; //don't clear W1C bits
					csr0 |= CSR0_FLUSHFIFO;
					os_writel(U3D_EP0CSR, csr0);

					os_printk(K_DEBUG, "waiting for flushfifo.....");
					while(os_readl(U3D_EP0CSR) & CSR0_FLUSHFIFO) //proceed before it clears
					{
						cpu_relax();
					}
					os_printk(K_DEBUG, "done.\n");
					#else
					//toggle EP0_RST
					os_setmsk(U3D_EP_RST, EP0_RST);
					os_clrmsk(U3D_EP_RST, EP0_RST);
					#endif
				}


				if(musb->ackpend & EP0_DATAEND)
				{
					os_printk(K_DEBUG, "Do not send dataend due to stall.\n");
					musb->ackpend &= ~EP0_DATAEND;
				}

				musb->ackpend |= EP0_SENDSTALL;
				musb->ep0_state = MUSB_EP0_STAGE_IDLE;
				os_printk(K_INFO, "@@@@@@@@@@@@@@@ SENDSTALL\n");
				os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE\n");
finish:
                os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | musb->ackpend);
                musb->ackpend = 0;
			}
		}
		break;

	case MUSB_EP0_STAGE_ACKWAIT:
		/* This should not happen. But happens with tusb6010 with
		 * g_file_storage and high speed. Do nothing.
		 */
		retval = IRQ_HANDLED;
		break;

	default:
		/* "can't happen" */
		WARN_ON(1);
		os_printk(K_INFO, "@@@@@@@@@@@@@@@ SENDSTALL\n");

		//flushfifo before send SENDSTALL

		if (os_readl(U3D_EP0CSR) & EP0_TXPKTRDY) //try to flushfifo after clear sentstall
		{
			#if 0
			u32 csr0 = 0;

			csr0 = os_readl(U3D_EP0CSR);
			csr0 &= EP0_W1C_BITS; //don't clear W1C bits
			csr0 |= CSR0_FLUSHFIFO;
			os_writel(U3D_EP0CSR, csr0);

			os_printk(K_DEBUG, "waiting for flushfifo.....");
			while(os_readl(U3D_EP0CSR) & CSR0_FLUSHFIFO) //proceed before it clears
			{
				cpu_relax();
			}
			os_printk(K_DEBUG, "done.\n");
			#else
			//toggle EP0_RST
			os_setmsk(U3D_EP_RST, EP0_RST);
			os_clrmsk(U3D_EP_RST, EP0_RST);
			#endif
		}


		os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | EP0_SENDSTALL);


		musb->ep0_state = MUSB_EP0_STAGE_IDLE;
		break;
	}

	return retval;
}


static int
musb_g_ep0_enable(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc)
{
	/* always enabled */
	return -EINVAL;
}

static int musb_g_ep0_disable(struct usb_ep *e)
{
	/* always enabled */
	return -EINVAL;
}

static int
musb_g_ep0_queue(struct usb_ep *e, struct usb_request *r, gfp_t gfp_flags)
{
	struct musb_ep		*ep;
	struct musb_request	*req;
	struct musb		*musb;
	int			status;
	unsigned long		lockflags;

	if (!e || !r)
		return -EINVAL;

	os_printk(K_DEBUG, "%s\n", __func__);

	ep = to_musb_ep(e);
	musb = ep->musb;

	req = to_musb_request(r);
	req->musb = musb;
	req->request.actual = 0;
	req->request.status = -EINPROGRESS;
	req->tx = ep->is_in;

	spin_lock_irqsave(&musb->lock, lockflags);

	if (!list_empty(&ep->req_list)) {
		status = -EBUSY;
		goto cleanup;
	}

	switch (musb->ep0_state) {
	case MUSB_EP0_STAGE_RX:		/* control-OUT data */
	case MUSB_EP0_STAGE_TX:		/* control-IN data */
	case MUSB_EP0_STAGE_ACKWAIT:	/* zero-length data */
		status = 0;
		break;
	default:
		dev_dbg(musb->controller, "ep0 request queued in state %d\n",
				musb->ep0_state);
		status = -EINVAL;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&req->list, &ep->req_list);

	//dev_dbg(musb->controller, "queue to %s (%s), length=%d\n",
	os_printk(K_DEBUG, "queue to %s (%s), length=%d\n",
			ep->name, ep->is_in ? "IN/TX" : "OUT/RX",
			req->request.length);

	/* sequence #1, IN ... start writing the data */
	if (musb->ep0_state == MUSB_EP0_STAGE_TX)
		ep0_txstate(musb);

	/* sequence #3, no-data ... issue IN status */
	else if (musb->ep0_state == MUSB_EP0_STAGE_ACKWAIT) {
		if (req->request.length)
			status = -EINVAL;
		else {
			musb->ep0_state = MUSB_EP0_STAGE_STATUSIN;
            os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_STATUSIN\n");

			os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | musb->ackpend | EP0_DATAEND);

            musb->ackpend = 0;
			musb_g_ep0_giveback(ep->musb, r);
		}

	/* else for sequence #2 (OUT), caller provides a buffer
	 * before the next packet arrives.  deferred responses
	 * (after SETUP is acked) are racey.
	 */
	} else if (musb->ackpend) {
		os_writel(U3D_EP0CSR, (os_readl(U3D_EP0CSR) & EP0_W1C_BITS) | musb->ackpend);
		musb->ackpend = 0;
	}

cleanup:
	spin_unlock_irqrestore(&musb->lock, lockflags);
	return status;
}

static int musb_g_ep0_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	/* we just won't support this */
	return -EINVAL;
}

static int musb_g_ep0_halt(struct usb_ep *e, int value)
{
	struct musb_ep		*ep;
	struct musb		*musb;
	void __iomem		*base;
	unsigned long		flags;
	int			status;
	u32			csr;

	os_printk(K_INFO, "%s\n", __func__);

	if (!e || !value)
		return -EINVAL;

	ep = to_musb_ep(e);
	musb = ep->musb;
	base = musb->mregs;
	status = 0;

	spin_lock_irqsave(&musb->lock, flags);

	if (!list_empty(&ep->req_list)) {
		status = -EBUSY;
		goto cleanup;
	}

	csr = musb->ackpend;

	switch (musb->ep0_state) {

	/* Stalls are usually issued after parsing SETUP packet, either
	 * directly in irq context from setup() or else later.
	 */
	case MUSB_EP0_STAGE_TX:		/* control-IN data */
	case MUSB_EP0_STAGE_ACKWAIT:	/* STALL for zero-length data */
	case MUSB_EP0_STAGE_RX:		/* control-OUT data */
		csr = os_readl(U3D_EP0CSR);
		/* FALLTHROUGH */

	/* It's also OK to issue stalls during callbacks when a non-empty
	 * DATA stage buffer has been read (or even written).
	 */
	case MUSB_EP0_STAGE_STATUSIN:	/* control-OUT status */
	case MUSB_EP0_STAGE_STATUSOUT:	/* control-IN status */

		if (os_readl(U3D_EP0CSR) & EP0_TXPKTRDY) //try to flushfifo after clear sentstall
		{
			#if 0
			u32 csr0 = 0;

			csr0 = os_readl(U3D_EP0CSR);
			csr0 &= EP0_W1C_BITS; //don't clear W1C bits
			csr0 |= CSR0_FLUSHFIFO;
			os_writel(U3D_EP0CSR, csr0);

			os_printk(K_DEBUG, "waiting for flushfifo.....");
			while(os_readl(U3D_EP0CSR) & CSR0_FLUSHFIFO) //proceed before it clears
			{
				cpu_relax();
			}
			os_printk(K_DEBUG, "done.\n");
			#else
			//toggle EP0_RST
			os_setmsk(U3D_EP_RST, EP0_RST);
			os_clrmsk(U3D_EP_RST, EP0_RST);
			#endif
		}

		csr = (csr & EP0_W1C_BITS) | EP0_SENDSTALL;
		os_printk(K_INFO, "@@@@@@@@@@@@@@@ SENDSTALL\n");
		os_writel(U3D_EP0CSR, csr);
		musb->ep0_state = MUSB_EP0_STAGE_IDLE;
		os_printk(K_DEBUG, "----- ep0 state: MUSB_EP0_STAGE_IDLE\n");
		musb->ackpend = 0;
		break;
	default:
		dev_dbg(musb->controller, "ep0 can't halt in state %d\n", musb->ep0_state);
		status = -EINVAL;
	}

cleanup:
	spin_unlock_irqrestore(&musb->lock, flags);
	return status;
}

const struct usb_ep_ops musb_g_ep0_ops = {
	.enable		= musb_g_ep0_enable,
	.disable	= musb_g_ep0_disable,
	.alloc_request	= musb_alloc_request,
	.free_request	= musb_free_request,
	.queue		= musb_g_ep0_queue,
	.dequeue	= musb_g_ep0_dequeue,
	.set_halt	= musb_g_ep0_halt,
};
