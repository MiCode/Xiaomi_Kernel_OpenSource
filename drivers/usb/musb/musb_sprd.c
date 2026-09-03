// SPDX-License-Identifier: GPL-2.0
/**
 * musb-sprd.c - Unisoc MUSB Specific Glue layer
 *
 * Copyright (c) 2018 Unisoc Inc.
 * http://www.Unisoc.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "musb_sprd.h"

static void musb_sprd_release_all_request(struct musb *musb);
static void musb_sprd_disable_all_interrupts(struct musb *musb);
const char *musb_drd_state_string(enum musb_drd_state state)
{
	if (state >= ARRAY_SIZE(state_names))
		return "UNKNOWN";

	return state_names[state];
}

static int boot_charging;

#if IS_ENABLED(CONFIG_SPRD_USBM)
static bool is_slave = true;
#else
static bool is_slave;
#endif

#if IS_ENABLED(CONFIG_USB_DWC3_SPRD)
extern int dwc3_sprd_probe_finish(void);
#else
int dwc3_sprd_probe_finish(void)
{
	is_slave = false;
	return 1;
}
#endif

static void sprd_musb_enable(struct musb *musb)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);
	u8 pwr;
	u8 otgextcsr;
	u8 devctl = musb_readb(musb->mregs, MUSB_DEVCTL);

	/* soft connect */
	if (glue->id_state == MUSB_ID_GROUND && glue->dr_mode == USB_DR_MODE_HOST) {
		musb->is_active = 1;

		devctl |= MUSB_DEVCTL_SESSION;
		musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);
		otgextcsr = musb_readb(musb->mregs, MUSB_OTG_EXT_CSR);
		otgextcsr |= MUSB_HOST_FORCE_EN;
		if (musb->is_multipoint)
			otgextcsr |= MUSB_TX_CMPL_MODE;
		musb_writeb(musb->mregs, MUSB_OTG_EXT_CSR, otgextcsr);
		dev_info(glue->dev, "%s:HOST ENABLE %02x\n",
			__func__, devctl);
		musb->context.devctl = devctl;
	} else {
		if (glue->vbus_active &&
			musb->gadget_driver &&
			!is_host_active(musb) &&
			(glue->usb_data_enabled == 1)) {
			dev_info(glue->dev, "%s:DONOTHING\n", __func__);
		} else {
			pwr = musb_readb(musb->mregs, MUSB_POWER);
			pwr &= ~MUSB_POWER_SOFTCONN;
			dev_info(glue->dev, "%s:SOFTDISCONN\n", __func__);
			dev_info(glue->dev, "is_host %d\n", is_host_active(musb));
			musb_writeb(musb->mregs, MUSB_POWER, pwr);
		}
	}
}

static void sprd_musb_disable(struct musb *musb)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);

	dev_info(glue->dev, "%s: enter\n", __func__);
	if (is_slave)
		musb_writeb(musb->mregs, MUSB_DEVCTL, 0);
	/* for test mode plug out/plug in */
	musb_writeb(musb->mregs, MUSB_TESTMODE, 0x0);
}

static irqreturn_t sprd_musb_interrupt(int irq, void *__hci)
{
	irqreturn_t retval = IRQ_NONE;
	struct musb *musb = __hci;
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);
	u32 reg_dma;
	u16 mask16;
	u8 mask8;

	spin_lock(&glue->lock);

	/* In order to implement 2nd charger detection
	 * initialize musb controller, so musb IRQ may
	 * happen during 2nd charger detection flow.
	 * In this case: USB handler clear IRQ & SOFT_CONN.
	 */
	if (glue->retry_charger_detect) {
		spin_unlock(&glue->lock);
		mask8 = musb_readb(musb->mregs, MUSB_POWER);
		mask8 &= ~MUSB_POWER_SOFTCONN;
		musb_writeb(musb->mregs, MUSB_POWER, mask8);
		dev_err(musb->controller,
			"interrupt status: 0x%x 0x%x - 0x%x 0x%x - 0x%x 0%x\n",
			 musb_readb(musb->mregs, MUSB_INTRUSBE),
			 musb_readb(musb->mregs, MUSB_INTRUSB),
			 musb_readw(musb->mregs, MUSB_INTRTXE),
			 musb_readw(musb->mregs, MUSB_INTRTX),
			 musb_readw(musb->mregs, MUSB_INTRRXE),
			 musb_readw(musb->mregs, MUSB_INTRRX));
		return retval;
	}

	if (atomic_read(&glue->musb_runtime_suspended)) {
		spin_unlock(&glue->lock);
		dev_err(musb->controller,
			"interrupt is already cleared!\n");
		return retval;
	}

	spin_lock(&musb->lock);
	mask8 = musb_readb(musb->mregs, MUSB_INTRUSBE);
	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB) & mask8;

	mask16 = musb_readw(musb->mregs, MUSB_INTRTXE);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX) & mask16;

	mask16 = musb_readw(musb->mregs, MUSB_INTRRXE);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX) & mask16;

	reg_dma = musb_readl(musb->mregs, MUSB_DMA_INTR_MASK_STATUS);

	dev_dbg(musb->controller, "%s usb%04x tx%04x rx%04x dma%x mask8%04x\n", __func__,
			musb->int_usb, musb->int_tx, musb->int_rx, reg_dma, mask8);

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval = musb_interrupt(musb);

#if IS_ENABLED(CONFIG_USB_SPRD_DMA)
	if (reg_dma)
		retval = sprd_dma_interrupt(musb, reg_dma);
#endif
	spin_unlock(&musb->lock);
	spin_unlock(&glue->lock);

	return retval;
}

static int sprd_musb_role_switch_set(struct sprd_glue *glue,
					enum usb_role role, bool force_set)
{
	struct musb *musb = glue->musb;
	enum usb_role new_role;
	u8 otgextcsr;
	u8 devctl;
	u8 power;
	int error = 0;

	if (role == glue->role && !force_set)
		return 0;

	switch (role) {
	case USB_ROLE_HOST:
		error = musb_set_host(musb);
		if (error)
			return error;

		new_role = USB_ROLE_HOST;

		/* sprd otg ext csr config */
		otgextcsr = musb_readb(musb->mregs, MUSB_OTG_EXT_CSR);
		otgextcsr |= MUSB_HOST_FORCE_EN;
		if (musb->is_multipoint)
			otgextcsr |= MUSB_TX_CMPL_MODE;
		musb_writeb(musb->mregs, MUSB_OTG_EXT_CSR, otgextcsr);

		devctl = readb(musb->mregs + MUSB_DEVCTL);
		dev_info(glue->dev, "HOST ENABLE %02x\n", devctl);
		break;
	case USB_ROLE_DEVICE:
		error = musb_set_peripheral(musb);
		if (error)
			return error;

		/* set MUSB POWER */
		power = musb_readb(musb->mregs, MUSB_POWER);
		if (musb->gadget_driver) {
			power |= MUSB_POWER_SOFTCONN;
			dev_info(glue->dev, "ROLE Device:SOFTCONN\n");
		} else {
			power &= ~MUSB_POWER_SOFTCONN;
			dev_info(glue->dev, "ROLE Device:SOFTDISCONN\n");
		}
		musb_writeb(musb->mregs, MUSB_POWER, power);
		new_role = USB_ROLE_DEVICE;
		devctl = readb(musb->mregs + MUSB_DEVCTL);
		dev_info(glue->dev, "DEVICE ENABLE %02x, power %02x\n", devctl, power);
		break;
	case USB_ROLE_NONE:
		new_role = USB_ROLE_NONE;

		devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
		devctl &= ~MUSB_DEVCTL_SESSION;
		musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);
		musb->xceiv->otg->default_a = 0;
		musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		MUSB_DEV_MODE(musb);

		dev_info(glue->dev, "CLEAR SESSION %02x\n", devctl);
		break;
	default:
		dev_err(glue->dev, "Invalid State\n");
		return -EINVAL;
	}

	glue->role = new_role;
	return 0;
}

static int musb_usb_role_switch_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct sprd_glue *glue = usb_role_switch_get_drvdata(sw);

	return sprd_musb_role_switch_set(glue, role, false);
}

static enum usb_role musb_usb_role_switch_get(struct usb_role_switch *sw)
{
	struct sprd_glue *glue = usb_role_switch_get_drvdata(sw);

	return glue->role;
}

static int sprd_musb_role_switch_init(struct sprd_glue *glue)
{
	struct usb_role_switch_desc role_sx_desc = { 0 };

	role_sx_desc.set = musb_usb_role_switch_set;
	role_sx_desc.get = musb_usb_role_switch_get;
	role_sx_desc.fwnode = dev_fwnode(glue->dev);
	role_sx_desc.driver_data = glue;
	glue->role_sw = usb_role_switch_register(glue->dev, &role_sx_desc);

	return PTR_ERR_OR_ZERO(glue->role_sw);
}

/* for some reason disable it now, fix me
static int sprd_musb_set_mode(struct musb *musb, u8 mode)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);
	enum usb_role new_role;

	switch (mode) {
	case MUSB_HOST:
		new_role = USB_ROLE_HOST;
		break;
	case MUSB_PERIPHERAL:
		new_role = USB_ROLE_DEVICE;
		break;
	case MUSB_OTG:
		new_role = USB_ROLE_NONE;
		break;
	default:
		dev_err(glue->dev, "Invalid mode request\n");
		return -EINVAL;
	}

	return sprd_musb_role_switch_set(glue, new_role, true);
}
*/

static int sprd_musb_init(struct musb *musb)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);
	int ret;

	glue->musb = musb;
	musb->phy = glue->phy;
	musb->xceiv = glue->xceiv;

	ret = sprd_musb_role_switch_init(glue);
	if (ret) {
		dev_err(glue->dev, "otg switch init failed!\n");
		//Todo:7731e init fail.First mask it then recover it after finding the cause.
		//return ret;
	}

	ret = usb_phy_init(glue->xceiv);
	if (ret != 0) {
		dev_err(glue->dev, "usb phy init failed!\n");
		usb_role_switch_unregister(glue->role_sw);
		return ret;
	}
	if (!is_slave)
		sprd_musb_enable(musb);

	musb->isr = sprd_musb_interrupt;
	return 0;
}

static int sprd_musb_exit(struct musb *musb)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);

	if (glue->usbid_irq)
		disable_irq_nosync(glue->usbid_irq);
	disable_irq_nosync(glue->vbus_irq);

	return 0;
}

static void sprd_musb_set_vbus(struct musb *musb, int is_on)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);
	struct usb_otg *otg = musb->xceiv->otg;
	u8 devctl;
	unsigned long timeout = 0;

	if (pm_runtime_suspended(musb->controller))
		return;

	devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
	dev_info(musb->controller, "is_on %d otg->state %d.\n",
			is_on, musb->xceiv->otg->state);

	if (is_on) {
		if (musb->xceiv->otg->state == OTG_STATE_A_IDLE) {
			/* start the session */
			devctl |= MUSB_DEVCTL_SESSION;
			musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);
			/*
			 * Wait for the musb to set as A
			 * device to enable the VBUS
			 */
			while (musb_readb(musb->mregs, MUSB_DEVCTL) &
					 MUSB_DEVCTL_BDEVICE) {
				if (++timeout > 1000) {
					dev_err(musb->controller,
					"configured as A device timeout");
					break;
				}
			}

			otg_set_vbus(otg, 1);
		} else {
			musb->is_active = 1;
			otg->default_a = 1;
			musb->xceiv->otg->state = OTG_STATE_A_WAIT_VRISE;
			devctl |= MUSB_DEVCTL_SESSION;
		}
	} else {
		musb->is_active = 0;

		/* If ID pin is grounded, we try to recover host mode */
		if (glue->id_state == MUSB_ID_GROUND) {
			dev_info(glue->dev, "try to recover musb controller\n");
			glue->host_recover = true;
			queue_work(glue->musb_wq, &glue->resume_work);
		} else {
			otg->default_a = 0;
			musb->xceiv->otg->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		}
		devctl &= ~MUSB_DEVCTL_SESSION;
	}
	musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);

	dev_dbg(musb->controller, "VBUS %s, devctl %02x\n",
		usb_otg_state_string(musb->xceiv->otg->state),
		musb_readb(musb->mregs, MUSB_DEVCTL));
}

static void sprd_musb_try_idle(struct musb *musb, unsigned long timeout)
{
	u8 otgextcsr;
	u16 txcsr;
	u32 i;
	void __iomem *mbase = musb->mregs;
	u32 csr;

	pr_info("%s enter, otg->state %d.\n",
			__func__, musb->xceiv->otg->state);

	if (musb->xceiv->otg->state == OTG_STATE_A_WAIT_BCON) {
		for (i = 1; i < musb->nr_endpoints; i++) {
			csr = musb_readl(mbase, MUSB_DMA_CHN_INTR(i));
			csr |= CHN_CLEAR_INT_EN;
			musb_writel(mbase, MUSB_DMA_CHN_INTR(i), csr);

			csr = musb_readl(mbase, MUSB_DMA_CHN_PAUSE(i));
			csr |= CHN_CLR;
			musb_writel(mbase, MUSB_DMA_CHN_PAUSE(i), csr);
		}

		otgextcsr = musb_readb(musb->mregs, MUSB_OTG_EXT_CSR);
		otgextcsr |= MUSB_CLEAR_TXBUFF | MUSB_CLEAR_RXBUFF;
		musb_writeb(musb->mregs, MUSB_OTG_EXT_CSR, otgextcsr);

		for (i = 0; i < musb->nr_endpoints; i++) {
			struct musb_hw_ep *hw_ep = musb->endpoints + i;

			txcsr = musb_readw(hw_ep->regs, MUSB_TXCSR);
			if (txcsr & MUSB_TXCSR_FIFONOTEMPTY) {
				txcsr |= MUSB_TXCSR_FLUSHFIFO;
				txcsr &= ~MUSB_TXCSR_TXPKTRDY;
				musb_writew(hw_ep->regs, MUSB_TXCSR, txcsr);
				musb_writew(hw_ep->regs, MUSB_TXCSR, txcsr);
				txcsr = musb_readw(hw_ep->regs, MUSB_TXCSR);
				txcsr &= ~(MUSB_TXCSR_AUTOSET
				| MUSB_TXCSR_DMAENAB
				| MUSB_TXCSR_DMAMODE
				| MUSB_TXCSR_H_RXSTALL
				| MUSB_TXCSR_H_NAKTIMEOUT
				| MUSB_TXCSR_H_ERROR
				| MUSB_TXCSR_TXPKTRDY);
				musb_writew(hw_ep->regs, MUSB_TXCSR, txcsr);
			}
		}
	}
}

static int sprd_musb_recover(struct musb *musb)
{
	struct sprd_glue *glue = dev_get_drvdata(musb->controller->parent);

	if (is_host_active(musb) && glue->dr_mode == USB_DR_MODE_HOST) {
		dev_info(glue->dev, "try to recover musb controller\n");
		glue->host_recover = true;
		queue_work(glue->musb_wq, &glue->resume_work);
	}

	return 0;
}

static const struct musb_platform_ops sprd_musb_ops = {
#if IS_ENABLED(CONFIG_SPRD_USBM)
	.quirks = MUSB_DMA_SPRD | MUSB_PRESERVE_SESSION,
#else
	.quirks = MUSB_DMA_SPRD,
#endif
	.init = sprd_musb_init,
	.exit = sprd_musb_exit,
	.enable = sprd_musb_enable,
	.disable = sprd_musb_disable,
#if IS_ENABLED(CONFIG_USB_SPRD_DMA)
	.dma_init = sprd_musb_dma_controller_create,
	.dma_exit = sprd_musb_dma_controller_destroy,
#endif
	.set_vbus = sprd_musb_set_vbus,
	.try_idle = sprd_musb_try_idle,
	.recover = sprd_musb_recover,
};

/* don't need to define sprd max ep num specially
 * better to use MUSB_C_NUM_EPS defined in musb_core.h
 * #define SPRD_MUSB_MAX_EP_NUM	16
 */
#define SPRD_MUSB_RAM_BITS	13
static struct musb_fifo_cfg sprd_musb_device_mode_cfg[] = {
	MUSB_EP_FIFO_DOUBLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(4, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(4, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(5, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(5, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(6, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(6, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(7, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(7, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(8, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(8, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(9, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(9, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(10, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(10, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(11, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(11, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(12, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(12, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(13, FIFO_TX, 1024),
	MUSB_EP_FIFO_DOUBLE(13, FIFO_RX, 1024),
	MUSB_EP_FIFO_DOUBLE(14, FIFO_TX, 256),
	MUSB_EP_FIFO_DOUBLE(14, FIFO_RX, 256),
	MUSB_EP_FIFO_DOUBLE(15, FIFO_TX, 256),
	MUSB_EP_FIFO_DOUBLE(15, FIFO_RX, 256),
};

static struct musb_fifo_cfg sprd_musb_host_mode_cfg[] = {
	MUSB_EP_FIFO_DOUBLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(4, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(4, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(5, FIFO_RX, 4096),
	MUSB_EP_FIFO_DOUBLE(6, FIFO_TX, 1024),
	MUSB_EP_FIFO_DOUBLE(6, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(7, FIFO_TX, 1024),
	MUSB_EP_FIFO_DOUBLE(7, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(8, FIFO_TX, 1024),
	MUSB_EP_FIFO_DOUBLE(8, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(9, FIFO_TX, 1024),
	MUSB_EP_FIFO_DOUBLE(9, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(10, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(10, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(11, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(11, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(12, FIFO_TX, 512),
	MUSB_EP_FIFO_DOUBLE(12, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(13, FIFO_TX, 8),
	MUSB_EP_FIFO_DOUBLE(13, FIFO_RX, 8),
	MUSB_EP_FIFO_DOUBLE(14, FIFO_TX, 8),
	MUSB_EP_FIFO_DOUBLE(14, FIFO_RX, 8),
	MUSB_EP_FIFO_DOUBLE(15, FIFO_TX, 8),
	MUSB_EP_FIFO_DOUBLE(15, FIFO_RX, 8),
};

static struct musb_fifo_cfg sprd_musb_device_mode_cfg_single[] = {
	MUSB_EP_FIFO_DOUBLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_DOUBLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(6, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(6, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(7, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(7, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(8, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(8, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(9, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(9, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(10, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(10, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(11, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(11, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(12, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(12, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(13, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(13, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(14, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(14, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(15, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(15, FIFO_RX, 512),
};
static struct musb_fifo_cfg sprd_musb_host_mode_cfg_single[] = {
	MUSB_EP_FIFO_SINGLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(4, FIFO_RX, 4096),
	MUSB_EP_FIFO_SINGLE(5, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(6, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(6, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(7, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(7, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(8, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(8, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(9, FIFO_TX, 1024),
	MUSB_EP_FIFO_SINGLE(9, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(10, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(10, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(11, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(11, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(12, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(12, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(13, FIFO_TX, 8),
	MUSB_EP_FIFO_SINGLE(13, FIFO_RX, 8),
	MUSB_EP_FIFO_SINGLE(14, FIFO_TX, 8),
	MUSB_EP_FIFO_SINGLE(14, FIFO_RX, 8),
	MUSB_EP_FIFO_SINGLE(15, FIFO_TX, 8),
	MUSB_EP_FIFO_SINGLE(15, FIFO_RX, 8),
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static struct musb_hdrc_config sprd_musb_hdrc_config = {
	.fifo_cfg = sprd_musb_device_mode_cfg,
	.fifo_cfg_size = ARRAY_SIZE(sprd_musb_device_mode_cfg),
	.multipoint = false,
	.dyn_fifo = true,
	.num_eps = MUSB_C_NUM_EPS,
	.ram_bits = SPRD_MUSB_RAM_BITS,
};

static struct musb_hdrc_config sprd_musb_hdrc_config_single = {
	.fifo_cfg = sprd_musb_device_mode_cfg_single,
	.fifo_cfg_size = ARRAY_SIZE(sprd_musb_device_mode_cfg_single),
	.multipoint = false,
	.dyn_fifo = true,
	.num_eps = MUSB_C_NUM_EPS,
	.ram_bits = SPRD_MUSB_RAM_BITS,
};
#pragma GCC diagnostic pop

static void sprd_relax_wakelock_timer(struct timer_list *timer)
{
	struct sprd_glue *glue = container_of(timer, struct sprd_glue, relax_wakelock_timer);
	unsigned long flags;

	spin_lock_irqsave(&glue->lock, flags);
	if (!glue->wake_lock_relaxed) {
		dev_info(glue->dev, "relax wakelock after timer expired\n");
		__pm_relax(glue->wake_lock);
		glue->wake_lock_relaxed = true;
	}
	spin_unlock_irqrestore(&glue->lock, flags);
}

static int musb_sprd_vbus_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sprd_glue *glue = container_of(nb, struct sprd_glue, vbus_nb);

	if (is_slave) {
		if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing())
			glue->xceiv->last_event = USB_EVENT_ID;
		else
			glue->xceiv->last_event = USB_EVENT_NONE;
		dev_info(glue->dev, "%s, event(%ld) ignored in slave mode\n", __func__, event);
		return NOTIFY_DONE;
	}

	if (glue->vbus_active == event) {
		dev_info(glue->dev, "ignore repeate vbus event.\n");
		return NOTIFY_DONE;
	}

	if (glue->id_state == MUSB_ID_GROUND) {
		dev_info(glue->dev, "ignore vbus state in id ground mode\n");
		return NOTIFY_DONE;
	}

	if (glue->dr_mode == USB_DR_MODE_HOST && !glue->use_pdhub_c2c) {
		dev_info(glue->dev, "ignore vbus state in dr mode host\n");
		return NOTIFY_DONE;
	}

	if (glue->drd_state == DRD_STATE_HOST_IDLE && !glue->use_pdhub_c2c) {
		dev_info(glue->dev, "ignore vbus state in host idle\n");
		return NOTIFY_DONE;
	}

	if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing() &&
		!sc27xx_get_current_status_detach_or_attach()) {
		dev_info(glue->dev, "ignore dr_swap: vbus, typec has been out.\n");
		return NOTIFY_DONE;
	}

	dev_info(glue->dev, "vbus:%ld event received\n", event);

	glue->vbus_active = event;

	if (glue->vbus_active && glue->chg_state == USB_CHG_STATE_UNDETECT) {
		if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing()) {
			glue->chg_state = USB_CHG_STATE_DETECTED;
		} else {
			glue->xceiv->last_event = USB_EVENT_VBUS;
			queue_delayed_work(glue->sm_usb_wq, &glue->chg_detect_work, 0);
			return NOTIFY_DONE;
		}
	}

	if (!glue->vbus_active) {
		flush_delayed_work(&glue->chg_detect_work);
		glue->xceiv->last_event = USB_EVENT_NONE;
		glue->chg_state = USB_CHG_STATE_UNDETECT;
		glue->charging_mode = false;
		glue->retry_chg_detect_count = 0;
		glue->wait_chg_detect_count = 0;
	}

	queue_work(glue->musb_wq, &glue->resume_work);

	return NOTIFY_DONE;
}

static int musb_sprd_id_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sprd_glue *glue = container_of(nb, struct sprd_glue, id_nb);
	enum musb_vbus_id_status id;
	unsigned long flags;

	id = event ? MUSB_ID_GROUND : MUSB_ID_FLOAT;

	/* to set USB_EVENT_ID as soon as possible */
	if (is_slave) {
		if (id == MUSB_ID_GROUND)
			glue->xceiv->last_event = USB_EVENT_ID;
		else
			glue->xceiv->last_event = USB_EVENT_NONE;

		if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing())
			glue->xceiv->last_event = USB_EVENT_ID;

		dev_info(glue->dev, "%s, event(%ld) ignored in slave mode\n", __func__, event);
		return NOTIFY_DONE;
	}

	if (glue->id_state == id) {
		dev_info(glue->dev, "ignore repeate id event.\n");
		return NOTIFY_DONE;
	}

	if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing() &&
		!sc27xx_get_current_status_detach_or_attach()) {
		dev_info(glue->dev, "ignore dr_swap: id, typec has been out.\n");
		return NOTIFY_DONE;
	}

	dev_info(glue->dev, "host:%ld (id:%d) event received\n", event, id);

	spin_lock_irqsave(&glue->lock, flags);
	glue->id_state = id;
	if (glue->id_state == MUSB_ID_GROUND) {
		glue->xceiv->last_event = USB_EVENT_ID;
	} else {
		glue->chg_state = USB_CHG_STATE_UNDETECT;
		glue->vbus_active = false;
		glue->charging_mode = false;
		glue->retry_chg_detect_count = 0;
		if (glue->use_pdhub_c2c && sc27xx_get_dr_swap_executing())
			glue->xceiv->last_event = USB_EVENT_ID;
		else
			glue->xceiv->last_event = USB_EVENT_NONE;
	}

	if (glue->wake_lock_relaxed) {
		mod_timer(&glue->relax_wakelock_timer, jiffies + RELAX_WAKE_LOCK_DELAY);
		__pm_stay_awake(glue->wake_lock);
		glue->wake_lock_relaxed = false;
		spin_unlock_irqrestore(&glue->lock, flags);
	} else {
		spin_unlock_irqrestore(&glue->lock, flags);
		del_timer_sync(&glue->relax_wakelock_timer);
		mod_timer(&glue->relax_wakelock_timer, jiffies + RELAX_WAKE_LOCK_DELAY);
	}

	queue_work(glue->musb_wq, &glue->resume_work);
	return NOTIFY_DONE;
}

static int musb_sprd_audio_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct sprd_glue *glue = container_of(nb, struct sprd_glue, audio_nb);
	unsigned long flags;

	if (glue->is_audio_dev == event) {
		dev_info(glue->dev, "ignore repeate audio event.\n");
		return NOTIFY_DONE;
	}

	dev_info(glue->dev, "audio:%ld event received\n", event);

	glue->is_audio_dev = event;

	spin_lock_irqsave(&glue->lock, flags);
	/* map audio event to id event */
	if (glue->is_audio_dev) {
		glue->xceiv->last_event = USB_EVENT_ID;
		glue->id_state = MUSB_ID_GROUND;
	} else {
		glue->xceiv->last_event = USB_EVENT_NONE;
		glue->id_state = MUSB_ID_FLOAT;
		glue->chg_state = USB_CHG_STATE_UNDETECT;
		glue->charging_mode = false;
		glue->retry_chg_detect_count = 0;
	}

	if (glue->wake_lock_relaxed) {
		mod_timer(&glue->relax_wakelock_timer, jiffies + RELAX_WAKE_LOCK_DELAY);
		__pm_stay_awake(glue->wake_lock);
		glue->wake_lock_relaxed = false;
		spin_unlock_irqrestore(&glue->lock, flags);
	} else {
		spin_unlock_irqrestore(&glue->lock, flags);
		del_timer_sync(&glue->relax_wakelock_timer);
		mod_timer(&glue->relax_wakelock_timer, jiffies + RELAX_WAKE_LOCK_DELAY);
	}

	queue_work(glue->musb_wq, &glue->resume_work);
	return NOTIFY_DONE;
}

static int musb_sprd_audio_high_speed_notifier(struct notifier_block *nb,
                    unsigned long event, void *data)
{
    struct sprd_glue *glue = container_of(nb, struct sprd_glue, audio_high_speed_nb);

    dev_info(glue->dev, "audio speed:%ld\n", event);
    if (event)
        queue_work(glue->sm_usb_wq, &glue->audio_work);
    return NOTIFY_DONE;
}

static void musb_sprd_detect_cable(struct sprd_glue *glue)
{
	unsigned long flags;
	struct extcon_dev *id_ext = glue->id_edev ? glue->id_edev : glue->edev;

	spin_lock_irqsave(&glue->lock, flags);
	if (extcon_get_state(id_ext, EXTCON_USB_HOST) == true) {
		dev_info(glue->dev, "host connection detected from ID GPIO.\n");
		glue->id_state = MUSB_ID_GROUND;
		glue->xceiv->last_event = USB_EVENT_ID;
		queue_work(glue->musb_wq, &glue->resume_work);
	} else if (extcon_get_state(glue->edev, EXTCON_USB) == true) {
		dev_info(glue->dev, "device connection detected from VBUS GPIO.\n");
		glue->vbus_active = true;
		glue->xceiv->last_event = USB_EVENT_VBUS;
		if (glue->vbus_active && glue->chg_state == USB_CHG_STATE_UNDETECT) {
			queue_delayed_work(glue->sm_usb_wq, &glue->chg_detect_work, 0);
			spin_unlock_irqrestore(&glue->lock, flags);
			return;
		}
		queue_work(glue->musb_wq, &glue->resume_work);
	}
	spin_unlock_irqrestore(&glue->lock, flags);
}

static enum usb_charger_type
musb_sprd_retry_charger_detect(struct sprd_glue *glue)
{
	struct musb *musb = glue->musb;
	struct usb_phy *usb_phy = glue->xceiv;
	unsigned long flags;
	u8 pwr;

	glue->chg_type = UNKNOWN_TYPE;
	dev_info(glue->dev, "%s enter\n", __func__);
	spin_lock_irqsave(&glue->lock, flags);
	glue->retry_charger_detect = true;
	spin_unlock_irqrestore(&glue->lock, flags);

	pm_runtime_get_sync(glue->dev);

	musb_writeb(musb->mregs, MUSB_INTRUSBE, 0);
	musb_writeb(musb->mregs, MUSB_INTRTXE, 0);
	musb_writeb(musb->mregs, MUSB_INTRRXE, 0);
	pwr = musb_readb(musb->mregs, MUSB_POWER);
	pwr |= MUSB_POWER_SOFTCONN;
	musb_writeb(musb->mregs, MUSB_POWER, pwr);

	/* because of GKI1.0, retry_charger_detect is intead of below */
	usb_phy->flags |= CHARGER_2NDDETECT_SELECT;
	glue->chg_type = usb_phy->charger_detect(glue->xceiv);
	usb_phy->flags &= ~CHARGER_2NDDETECT_SELECT;

	pwr = musb_readb(musb->mregs, MUSB_POWER);
	pwr &= ~MUSB_POWER_SOFTCONN;
	musb_writeb(musb->mregs, MUSB_POWER, pwr);
	/*  flush pending interrupts */
	spin_lock_irqsave(&glue->lock, flags);
	glue->retry_charger_detect = false;
	spin_unlock_irqrestore(&glue->lock, flags);
	musb_readb(musb->mregs, MUSB_INTRUSB);
	musb_readw(musb->mregs, MUSB_INTRTXE);

	pm_runtime_mark_last_busy(glue->dev);
	pm_runtime_put_autosuspend(glue->dev);

	return glue->chg_type;
}

static void musb_sprd_charger_mode(void)
{
	struct device_node *np;
	const char *cmd_line, *s;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return;

	s = strstr(cmd_line, "androidboot.mode=charger");
	if (s != NULL)
		boot_charging = 1;
	else {
		s = strstr(cmd_line, "sprdboot.mode=charger");
		if (s != NULL)
			boot_charging = 1;
		else
			boot_charging = 0;
	}
}

static void sprd_musb_reset_context(struct musb *musb)
{
	int i;

	musb->context.testmode = 0;
	musb->test_mode_nr = 0;
	musb->test_mode = false;
	for (i = 0; i < musb->config->num_eps; ++i) {
		musb->context.index_regs[i].txcsr = 0;
		musb->context.index_regs[i].rxcsr = 0;
	}
}

static void sprd_musb_reset_controller(struct sprd_glue *glue)
{
	struct musb *musb = glue->musb;
	int ret = 0;

	spin_lock(&musb->lock);
	usb_phy_vbus_off(glue->xceiv);
	musb_sprd_disable_all_interrupts(musb);
	glue->xceiv->flags |= SPRD_USB_PHY_IGNORE_RESET;
	usb_phy_shutdown(glue->xceiv);
	ret = usb_phy_init(glue->xceiv);
	glue->xceiv->flags &= ~SPRD_USB_PHY_IGNORE_RESET;
	spin_unlock(&musb->lock);
	if (ret != 0)
		dev_warn(glue->dev, "usb phy init abnormal %d\n", ret);
}

#if IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER)
static bool musb_sprd_lowpower_configuration_onoff(struct sprd_glue *glue, int on)
{
	struct musb *musb = glue->musb;
	u32 val = 0;

	if (glue->dr_mode == USB_DR_MODE_UNKNOWN || musb->offload_used) {
		dev_info(glue->dev, "not support musb lowpower due to unknown mode or offload!\n");
		return false;
	}

	/* check lowpower configuration condition*/
	if (!glue->pubsys_bypass.regmap_ptr)
		return false;
	if (!glue->suspend_clk_src_frc_on.regmap_ptr)
		return false;
	if (!glue->hclk_src_sel || !glue->hclk_suspend_src)
		return false;

	if (on) {
		/* make pubsys deep bypass */
		regmap_read(glue->pubsys_bypass.regmap_ptr,
				glue->pubsys_bypass.args[0], &val);
		val |= glue->pubsys_bypass.args[1];
		regmap_write(glue->pubsys_bypass.regmap_ptr,
				glue->pubsys_bypass.args[0], val);

		/* make suspend clk source force on */
		regmap_update_bits(glue->suspend_clk_src_frc_on.regmap_ptr,
				glue->suspend_clk_src_frc_on.args[0],
				glue->suspend_clk_src_frc_on.args[1],
				glue->suspend_clk_src_frc_on.args[1]);

		/* switch hclk to suspend clk source */
		clk_set_parent(glue->hclk_src_sel, glue->hclk_suspend_src);
		usb_phy_set_wakeup(glue->xceiv, 1);
	} else {
		/* cancel pubsys deep bypass */
		regmap_read(glue->pubsys_bypass.regmap_ptr,
				glue->pubsys_bypass.args[0], &val);
		val &= ~glue->pubsys_bypass.args[1];
		regmap_write(glue->pubsys_bypass.regmap_ptr,
				glue->pubsys_bypass.args[0], val);

		/* cancel suspend clk source force on */
		regmap_update_bits(glue->suspend_clk_src_frc_on.regmap_ptr,
				glue->suspend_clk_src_frc_on.args[0],
				glue->suspend_clk_src_frc_on.args[1],
				~glue->suspend_clk_src_frc_on.args[1]);

		/* switch hclk to default clk */
		clk_set_parent(glue->hclk_src_sel, glue->hclk_default_src);
		usb_phy_set_wakeup(glue->xceiv, 0);
	}

	return true;
}
#endif

/**
 * Show / Store the hostenable attribure.
 */
static ssize_t musb_hostenable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
		((glue->host_disabled & 0x01) ? "disabled" : "enabled"));
}

static ssize_t musb_hostenable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);

	if (strncmp(buf, "disable", 7) == 0) {
		glue->host_disabled |= 1;
		disable_irq(glue->usbid_irq);
	} else if (strncmp(buf, "enable", 6) == 0) {
		glue->host_disabled &= ~(0x01);
		enable_irq(glue->usbid_irq);
	} else {
		return 0;
	}

	return count;
}
DEVICE_ATTR_RW(musb_hostenable);

static ssize_t maximum_speed_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);
	struct musb *musb;

	if (!glue)
		return -EINVAL;

	musb = glue->musb;
	if (!musb)
		return -EINVAL;

	return sprintf(buf, "%s\n",
		usb_speed_string(musb->config->maximum_speed));
}

static ssize_t maximum_speed_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);
	struct musb *musb;
	u32 max_speed;

	if (!glue)
		return -EINVAL;

	if (kstrtouint(buf, 0, &max_speed))
		return -EINVAL;

	if (max_speed > USB_SPEED_SUPER)
		return -EINVAL;

	musb = glue->musb;
	if (!musb)
		return -EINVAL;

	if (glue->use_singlefifo)
		sprd_musb_hdrc_config_single.maximum_speed = max_speed;
	else
		sprd_musb_hdrc_config.maximum_speed = max_speed;
	musb->g.max_speed = max_speed;
	return size;
}
static DEVICE_ATTR_RW(maximum_speed);

static ssize_t current_speed_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);
	struct musb *musb;

	if (!glue)
		return -EINVAL;

	musb = glue->musb;
	if (!musb)
		return -EINVAL;

	return sprintf(buf, "%s\n", usb_speed_string(musb->g.speed));
}
static DEVICE_ATTR_RO(current_speed);

static struct attribute *musb_sprd_attrs[] = {
	&dev_attr_maximum_speed.attr,
	&dev_attr_current_speed.attr,
	&dev_attr_musb_hostenable.attr,
	NULL
};
ATTRIBUTE_GROUPS(musb_sprd);

static struct class *usb_notify_class;
static struct device *usb_notify_dev;

static ssize_t usb_data_enabled_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", glue->usb_data_enabled);
}

static ssize_t usb_data_enabled_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int value = 0;
	int ret = 0;
	unsigned long flags;
	struct sprd_glue *glue = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "input err:%d\n", ret);
		return count;
	}
	spin_lock_irqsave(&glue->lock, flags);
	dev_info(dev, "usb_data_enabled input: %d current: %d\n",
			value, glue->usb_data_enabled);

	if (glue->usb_data_enabled != value) {
		glue->usb_data_enabled = value;
		queue_work(glue->musb_wq, &glue->resume_work);
	}

	spin_unlock_irqrestore(&glue->lock, flags);

	return count;
}

static DEVICE_ATTR_RW(usb_data_enabled);

static struct attribute *usb_data_control_attrs[] = {
	&dev_attr_usb_data_enabled.attr,
	NULL
};

static const struct attribute_group usb_data_control_group = {
	.attrs = usb_data_control_attrs,
};

static int musb_sprd_usb_notify_init(struct platform_device *pdev, void *data)
{
	int ret = 0;

	usb_notify_class = class_create(THIS_MODULE, "usb_notify");
	if (IS_ERR_OR_NULL(usb_notify_class)) {
		dev_err(&pdev->dev, "usb_notify class create err.\n");
		ret = PTR_ERR(usb_notify_class);
		goto out;
	}

	usb_notify_dev =
		device_create(usb_notify_class, &pdev->dev, 0, NULL, "usb_control");
	if (IS_ERR_OR_NULL(usb_notify_dev)) {
		dev_err(&pdev->dev, "usb_notify class create err.\n");
		ret = PTR_ERR(usb_notify_dev);
		class_destroy(usb_notify_class);
		goto out;
	}

	ret = sysfs_create_group(&usb_notify_dev->kobj, &usb_data_control_group);
	if (ret) {
		dev_err(&pdev->dev, "sysfs create err. ret:%d\n", ret);
		device_destroy(usb_notify_class, usb_notify_dev->devt);
		class_destroy(usb_notify_class);
		goto out;
	}

	dev_set_drvdata(usb_notify_dev, data);
	dev_info(&pdev->dev, "[%s] --\n", __func__);

out:
	return ret;
}

static void musb_sprd_usb_notify_exit(struct platform_device *pdev)
{
	if (usb_notify_dev) {
		sysfs_remove_group(&usb_notify_dev->kobj, &usb_data_control_group);
		device_destroy(usb_notify_class, usb_notify_dev->devt);
	}

	if (usb_notify_class)
		class_destroy(usb_notify_class);

	dev_info(&pdev->dev, "[%s] --\n", __func__);
}

static int musb_sprd_is_udc_start(struct sprd_glue *glue)
{
	struct musb *musb = glue->musb;
	unsigned long flags;

	spin_lock_irqsave(&musb->lock, flags);
	if (!musb->gadget_driver) {
		spin_unlock_irqrestore(&musb->lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&musb->lock, flags);
	return 1;
}

static void musb_sprd_start_gadget(struct musb *musb)
{
	u8 devctl;
	void __iomem *regs = musb->mregs;
	u8 power;

	/*  Set INT enable registers, enable interrupts */
	musb->intrtxe = musb->epmask;
	musb_writew(regs, MUSB_INTRTXE, musb->intrtxe);
	musb->intrrxe = musb->epmask & 0xfffe;
	musb_writew(regs, MUSB_INTRRXE, musb->intrrxe);
	musb_writeb(regs, MUSB_INTRUSBE, 0xf7);

	musb_writeb(regs, MUSB_TESTMODE, 0);

	power = MUSB_POWER_ISOUPDATE;
	/*
	 * treating UNKNOWN as unspecified maximum speed, in which case
	 * we will default to high-speed.
	 */
	if (musb->config->maximum_speed == USB_SPEED_HIGH ||
			musb->config->maximum_speed == USB_SPEED_UNKNOWN)
		power |= MUSB_POWER_HSENAB;

	/*
	 * for suspend/resume feature, we should set POWER.Enable_SuspendM to enable
	 * SUSPENDM output, which can control phy to enter into suspend mode.
	 */
	power |= MUSB_POWER_ENSUSPEND;

	musb_writeb(regs, MUSB_POWER, power);

	devctl = musb_readb(regs, MUSB_DEVCTL);
	devctl &= ~MUSB_DEVCTL_SESSION;

	musb_writeb(regs, MUSB_DEVCTL, devctl);
}

/**
 * musb_sprd_otg_start_peripheral -  bind/unbind the peripheral controller.
 *
 * @glue: Pointer to the musb_sprd structure.
 * @on:   Turn ON/OFF the gadget.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int musb_sprd_otg_start_peripheral(struct sprd_glue *glue, int on)
{
	struct musb *musb = glue->musb;

	if (on) {
		dev_info(glue->dev, "%s: turn on gadget %s\n", __func__, musb->g.name);

		pm_stay_awake(musb->controller);

		MUSB_DEV_MODE(musb);
		musb->is_active = 0;
		musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		if (glue->use_singlefifo)
			sprd_musb_hdrc_config_single.fifo_cfg = sprd_musb_device_mode_cfg_single;
		else
			sprd_musb_hdrc_config.fifo_cfg = sprd_musb_device_mode_cfg;
		sprd_musb_reset_context(musb);
		pm_runtime_get_sync(musb->controller);
		musb_reset_all_fifo_2_default(musb);

		usb_phy_vbus_off(glue->xceiv);
		/*
		 * Musb controller process go as device default.
		 * From asic,controller will wait 150ms and then check vbus
		 * if vbus is powered up.
		 * Session reg effects relay on vbus checked ok while seted.
		 * If not sleep,it will contine cost 150ms to check vbus ok
		 * before session take effect.Which may cause session effect
		 * timeout and usb switch to host failed Sometimes.
		 */
		//msleep(150);
		musb_sprd_start_gadget(musb);
		usb_udc_vbus_handler(&musb->g, true);
		flush_delayed_work(&musb->gadget_work);

		usb_gadget_set_state(&musb->g, USB_STATE_ATTACHED);
		glue->dr_mode = USB_DR_MODE_PERIPHERAL;
	} else {
		struct sprd_usb_udc *sprd_udc = (struct sprd_usb_udc *)musb->g.udc;
		struct usb_gadget *gadget = &musb->g;
		u8 devctl;
		dev_info(glue->dev, "%s: turn off gadget %s\n", __func__, musb->g.name);
	        devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
	        devctl &= ~MUSB_DEVCTL_SESSION;
	        musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);

		if (sprd_udc)
			sprd_udc->vbus = false;

		gadget->ops->pullup(gadget, 0);
		flush_delayed_work(&musb->gadget_work);

		musb_sprd_release_all_request(musb);
		usb_gadget_set_state(&musb->g, USB_STATE_NOTATTACHED);
		/*dp/dm change to others*/
		if (glue->use_pdhub_c2c)
			call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
				false, NULL);
		musb->offload_used = 0;
		pm_runtime_put_sync(musb->controller);

		pm_relax(musb->controller);
	}

	return 0;
}

#if defined(CONFIG_USB_SPRD_OFFLOAD)
static inline void musb_sprd_offload_shutdown(struct musb *musb)
{
	void __iomem	*mbase = musb->mregs;

	musb_writel(mbase, MUSB_AUDIO_IIS_DMA_CHN, 0);
}
#else
static inline void musb_sprd_offload_shutdown(struct musb *musb)
{
}
#endif

#if defined(CONFIG_USB_SPRD_ADAPTIVE)
static void musb_sprd_adaptive_shutdown(struct musb *musb)
{
	u32 val;
	void __iomem *mbase = musb->mregs;

	/* Disable the adaptive mode */
	val  = musb_readl(mbase, MUSB_DMA_FRAG_WAIT);
	val &= ~BIT_USB_AUDIO_ADP_MODE;
	musb_writel(mbase, MUSB_DMA_FRAG_WAIT, val);
	dev_info(musb->controller, "%s MUSB_DMA_FRAG_WAIT:0x%x\n",
		__func__, val);
}
#endif

/**
 * musb_sprd_otg_start_host -  helper function for starting/stopping the host
 * controller driver.
 *
 * @glue: Pointer to the sprd_glue structure.
 * @on: start / stop the host controller driver.
 *
 * Returns 0 on success otherwise negative errno.
 */
static int musb_sprd_otg_start_host(struct sprd_glue *glue, int on)
{
	int ret = 0;
	struct musb *musb = glue->musb;
	struct musb_hdrc_platform_data *plat = dev_get_platdata(musb->controller);

	if (!glue->vbus) {
		glue->vbus = devm_regulator_get(glue->dev, "vddvbus");
		if (IS_ERR(glue->vbus)) {
			glue->vbus = NULL;
			return -EPROBE_DEFER;
		}
		dev_info(glue->dev, "get vbus succeed\n");
	}

	if (on) {
		dev_info(glue->dev, "%s: turn on host\n", __func__);

		if (!glue->use_pdhub_c2c) {
			if (!regulator_is_enabled(glue->vbus)) {
				dev_info(glue->dev, "%s: regulator enable\n", __func__);
				ret = regulator_enable(glue->vbus);
				if (ret) {
					dev_err(glue->dev, "Failed to enable vbus: %d\n", ret);
					return ret;
				}
			}
		}

		if (!glue->enable_pm_suspend_in_host && !IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER))
			pm_stay_awake(musb->controller);

		if (glue->use_singlefifo)
			sprd_musb_hdrc_config_single.fifo_cfg = sprd_musb_host_mode_cfg_single;
		else
			sprd_musb_hdrc_config.fifo_cfg = sprd_musb_host_mode_cfg;
		sprd_musb_reset_context(musb);

		/* Increment pm usage count in host state.*/
		pm_runtime_get_sync(musb->controller);

		if (musb->port_mode != MUSB_HOST) {
			ret = musb_host_setup(musb, plat->power);
			if (ret) {
				dev_err(glue->dev, "musb_host_setup failed: %d\n", ret);
				regulator_disable(glue->vbus);
				pm_runtime_put_sync(musb->controller);
				return ret;
			}
		} else {
			/*
			 * for host only mode, musb_host_setup is already called
			 * in musb_init_controller.
			 * It's not removed in musb_init_controller because of EDIFIER earphone.
			 * This earphone sometimes is not recognized after
			 * musb_host_setup/musb_host_cleanup.
			 * It's only happened in N6P. N6L doesn't have this issue.
			 */
			dev_info(glue->dev, "host only mode\n");
		}
		MUSB_HST_MODE(musb);
		musb->xceiv->otg->state = OTG_STATE_A_IDLE;
		musb->hops.host_start(musb);
		musb_reset_all_fifo_2_default(musb);

		usb_phy_vbus_on(glue->xceiv);
		/*
		 * Musb controller process go as device default.
		 * From asic,controller will wait 150ms and then check vbus
		 * if vbus is powered up.
		 * Session reg effects relay on vbus checked ok while seted.
		 * If not sleep,it will contine cost 150ms to check vbus ok
		 * before session take effect.Which may cause session effect
		 * timeout and usb switch to host failed Sometimes.
		 */
		msleep(150);
		glue->dr_mode = USB_DR_MODE_HOST;
		sprd_musb_enable(musb);
	} else {
		u8 devctl;
		dev_info(glue->dev, "%s: turn off host\n", __func__);
	        devctl = musb_readb(musb->mregs, MUSB_DEVCTL);
	        devctl &= ~MUSB_DEVCTL_SESSION;
	        musb_writeb(musb->mregs, MUSB_DEVCTL, devctl);


		if (musb->port_mode != MUSB_HOST)
			musb_host_cleanup(musb);

		if (!glue->use_pdhub_c2c) {
			ret = regulator_disable(glue->vbus);
			if (ret)
				dev_err(glue->dev, "Failed to disable vbus: %d\n", ret);
		}

		/* disable usb audio offload */
		if (musb->is_offload) {
			dev_dbg(musb->controller, "disable audio channel\n");
			musb_sprd_offload_shutdown(musb);
			musb->is_offload = 0;
		}
		musb->offload_used = 0;
#if defined(CONFIG_USB_SPRD_ADAPTIVE)
		if (musb->is_adaptive) {
			dev_dbg(musb->controller, "disable adaptive channel\n");
			musb_sprd_adaptive_shutdown(musb);
		}
#endif

		glue->musb->is_active = 0;
		glue->dr_mode = USB_DR_MODE_UNKNOWN;
		/*dp/dm change to others*/
		if (glue->use_pdhub_c2c)
			call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
				false, NULL);

		/* Decrement pm usage count when leave host state.*/
		pm_runtime_put_sync(musb->controller);

		if (!glue->enable_pm_suspend_in_host && !IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER))
			pm_relax(musb->controller);
	}

	return 0;
}

static void musb_sprd_chg_detect_work(struct work_struct *work)
{
	struct sprd_glue *glue = container_of(work, struct sprd_glue, chg_detect_work.work);
	struct usb_phy *usb_phy = glue->xceiv;
	unsigned long delay = 0;
	bool rework = false;

	if (!glue->vbus_active) {
		dev_info(glue->dev, "musb:line%d: vbus_active 0\n", __LINE__);
		return;
	}

	switch (glue->chg_state) {
	case USB_CHG_STATE_UNDETECT:
		if (boot_charging) {
			dev_info(glue->dev, "boot charging mode enter!\n");
			glue->charging_mode = true;
			glue->xceiv->last_event = USB_EVENT_CHARGER;
			break;
		}
		glue->chg_state = USB_CHG_STATE_DETECT;
		fallthrough;
	case USB_CHG_STATE_DETECT:
		if (usb_phy->charger_detect)
			glue->chg_type = usb_phy->charger_detect(usb_phy);

		if (!glue->vbus_active) {
			dev_info(glue->dev, "musb:line%d: vbus_active 0\n", __LINE__);
			break;
		}

		if (!(usb_phy->flags & CHARGER_DETECT_DONE)
		    && glue->wait_chg_detect_count < MUSB_CHG_MAX_WAIT_BC1P2_COUNT) {
			dev_info(glue->dev, "musb:wait bc1.2 done");
			glue->wait_chg_detect_count++;
			rework = true;
			delay = MUSB_CHG_WAIT_DETECT_DELAY;
			break;
		}
		glue->chg_state = USB_CHG_STATE_DETECTED;
		fallthrough;
	case USB_CHG_STATE_DETECTED:
		dev_info(glue->dev, "charger = %d\n", glue->chg_type);
		if (glue->chg_type == UNKNOWN_TYPE) {
			if (usb_phy->flags & CHARGER_2NDDETECT_ENABLE) {
				glue->chg_state = USB_CHG_STATE_RETRY_DETECT;
				rework = true;
				delay = CHARGER_DETECT_DELAY;
			} else {
				dev_info(glue->dev, "charge detect finished\n");
				glue->xceiv->last_event = USB_EVENT_CHARGER;
				glue->charging_mode = true;
			}
		} else if (glue->chg_type == SDP_TYPE || glue->chg_type == CDP_TYPE) {
			dev_info(glue->dev, "charge detect finished with %d\n", glue->chg_type);
			glue->xceiv->last_event = USB_EVENT_ENUMERATED;
			queue_work(glue->musb_wq, &glue->resume_work);
		} else {
			dev_info(glue->dev, "charge detect finished\n");
			glue->xceiv->last_event = USB_EVENT_CHARGER;
			glue->charging_mode = true;
		}
		break;
	case USB_CHG_STATE_RETRY_DETECT:
		if (extcon_get_state(glue->edev, EXTCON_USB))
			glue->chg_type = musb_sprd_retry_charger_detect(glue);

		glue->chg_state = USB_CHG_STATE_RETRY_DETECTED;
		fallthrough;
	case USB_CHG_STATE_RETRY_DETECTED:
		dev_info(glue->dev, "redetected charger = %d\n", glue->chg_type);
		if (glue->chg_type == UNKNOWN_TYPE) {
			dev_info(glue->dev, "charge retry_detect finished\n");
			glue->xceiv->last_event = USB_EVENT_CHARGER;
			glue->charging_mode = true;
		} else if (glue->chg_type == SDP_TYPE || glue->chg_type == CDP_TYPE) {
			dev_info(glue->dev, "charge retry_detect finished with %d\n", glue->chg_type);
			glue->xceiv->last_event = USB_EVENT_ENUMERATED;
			queue_work(glue->musb_wq, &glue->resume_work);
		} else {
			dev_info(glue->dev, "charge retry_detect finished\n");
			glue->xceiv->last_event = USB_EVENT_CHARGER;
			glue->charging_mode = true;
		}
		break;
	default:
		return;
	}

	if (rework)
		queue_delayed_work(glue->sm_usb_wq, &glue->chg_detect_work, delay);
}

static void musb_sprd_audio_work(struct work_struct *work)
{
    struct sprd_glue *glue = container_of(work, struct sprd_glue, audio_work);

    if (glue->drd_state == DRD_STATE_HOST && glue->id_state == MUSB_ID_GROUND) {
        dev_info(glue->dev, "%s:start turn to full speed\n", __func__);

        if (glue->use_singlefifo)
            sprd_musb_hdrc_config_single.maximum_speed = USB_SPEED_FULL;
        else
            sprd_musb_hdrc_config.maximum_speed = USB_SPEED_FULL;
        musb_sprd_otg_start_host(glue, 0);
        while (!pm_runtime_suspended(glue->dev)) {
            dev_info(glue->dev, "%s: waiting glue suspended\n", __func__);
            msleep(20);
        }
        musb_sprd_otg_start_host(glue, 1);
        if (glue->use_singlefifo)
            sprd_musb_hdrc_config_single.maximum_speed = USB_SPEED_UNKNOWN;
        else
            sprd_musb_hdrc_config.maximum_speed = USB_SPEED_UNKNOWN;
    } else {
        const char *state = musb_drd_state_string(glue->drd_state);

        dev_err(glue->dev, "%s: err:%s state, id:%d\n",
            __func__, state, glue->id_state);
    }
}

static int musb_sprd_suspend(struct sprd_glue *glue)
{
	struct musb *musb = glue->musb;

	dev_info(glue->dev, "%s: enter\n", __func__);

	mutex_lock(&glue->suspend_resume_mutex);
	if (atomic_read(&glue->musb_runtime_suspended)) {
		dev_info(glue->dev, "%s: Already suspended\n", __func__);
		mutex_unlock(&glue->suspend_resume_mutex);
		return 0;
	}
	if (glue->drd_state == DRD_STATE_RUNTIME_SUSPENDING)
		usb_phy_vbus_off(glue->xceiv);
	atomic_set(&glue->musb_runtime_suspended, 1);
	musb_sprd_disable_all_interrupts(musb);
	clk_disable_unprepare(glue->clk);
	usb_phy_shutdown(glue->xceiv);
	mutex_unlock(&glue->suspend_resume_mutex);
	return 0;
}

static int musb_sprd_resume(struct sprd_glue *glue)
{
	int ret = 0;

	dev_info(glue->dev, "%s: enter\n", __func__);

	mutex_lock(&glue->suspend_resume_mutex);
	if (!atomic_read(&glue->musb_runtime_suspended)) {
		dev_info(glue->dev, "%s: Already resumed\n", __func__);
		mutex_unlock(&glue->suspend_resume_mutex);
		return 0;
	}

	ret = clk_prepare_enable(glue->clk);
	if (ret != 0)
		dev_warn(glue->dev, "clk prepare enable abnormal %d\n", ret);

	ret = usb_phy_init(glue->xceiv);
	if (ret != 0)
		dev_warn(glue->dev, "usb phy init abnormal %d\n", ret);

	atomic_set(&glue->musb_runtime_suspended, 0);
	mutex_unlock(&glue->suspend_resume_mutex);
	return 0;
}

/**
 * musb_sprd_ext_event_notify - callback to handle events from external transceiver
 *
 * Returns 0 on success
 */
static void musb_sprd_ext_event_notify(struct sprd_glue *glue)
{
	/* Flush processing any pending events before handling new ones */
	flush_delayed_work(&glue->sm_work);

	dev_info(glue->dev,
			"ext event: id %d, vbus %d, b_susp %d, a_recover %d, b_data %d\n",
			glue->id_state, glue->vbus_active,
			glue->gadget_suspend, glue->host_recover,
			glue->usb_data_enabled);

	if (glue->id_state == MUSB_ID_FLOAT)
		set_bit(ID, &glue->inputs);
	else
		clear_bit(ID, &glue->inputs);

	if (glue->vbus_active && !glue->in_restart)
		set_bit(B_SESS_VLD, &glue->inputs);
	else
		clear_bit(B_SESS_VLD, &glue->inputs);

	if (glue->gadget_suspend)
		set_bit(B_SUSPEND, &glue->inputs);
	else
		clear_bit(B_SUSPEND, &glue->inputs);

	if (glue->host_recover) {
		set_bit(A_RECOVER, &glue->inputs);
		glue->host_recover = false;
	}

	if (glue->usb_data_enabled == 0)
		set_bit(B_DATA_DISABLED, &glue->inputs);
	else
		clear_bit(B_DATA_DISABLED, &glue->inputs);

	queue_delayed_work(glue->sm_usb_wq, &glue->sm_work, 0);
}

static void musb_sprd_resume_work(struct work_struct *work)
{
	struct sprd_glue *glue = container_of(work, struct sprd_glue, resume_work);

	dev_dbg(glue->dev, "%s enter\n", __func__);

	/* in some scene, maybe here need to do resume first */
	/* musb_sprd_resume(glue); */

	if (atomic_read(&glue->pm_suspended)) {
		/*
		 * delay start sm_work in pm suspend state
		 * musb_sprd_pm_resume will kick the state machine later.
		 */
		dev_info(glue->dev, "delay start sm_work in pm suspend state\n");
		return;
	}

	if (glue->vbus_active) {
		if (glue->chg_state != USB_CHG_STATE_DETECTED &&
			glue->chg_state != USB_CHG_STATE_RETRY_DETECTED) {

			if (glue->id_state == MUSB_ID_GROUND) {
				dev_info(glue->dev, "waiting for vbus charger detect finished\n");
				msleep(20);
				queue_work(glue->musb_wq, &glue->resume_work);
			} else
				dev_info(glue->dev, "vbus charger detect not finished\n");

			return;
		}
	}

	if (glue->charging_mode || boot_charging) {
		if (glue->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(glue->xceiv, 500);

		dev_info(glue->dev, "don't need start sm_work in charging mode\n");
		return;
	}

	musb_sprd_ext_event_notify(glue);
}

/**
 * musb_sprd_otg_sm_work - workqueue function.
 *
 * @w: Pointer to the musb otg workqueue
 *
 * NOTE: After any change in drd_state, we must reschdule the state machine.
 */
static void musb_sprd_otg_sm_work(struct work_struct *work)
{
	struct sprd_glue *glue = container_of(work, struct sprd_glue, sm_work.work);
	const char *state;
	bool rework = false;
	unsigned long delay = 0;
	int ret = 0;

	state = musb_drd_state_string(glue->drd_state);
	dev_info(glue->dev, "sm_work: %s state\n", state);

	/* Check OTG state */
	switch (glue->drd_state) {
	case DRD_STATE_UNDEFINED:
		/*
		 * in probe, phy init, clk prepare, wakelock taken are done,
		 * so here just enable runtime, don't need to do actually resume,
		 * then let pm runtime to shutdown phy, unprepare clk and
		 * release wakelock in autosuspend.
		 */
		pm_runtime_set_active(glue->dev);
		pm_runtime_use_autosuspend(glue->dev);
		pm_runtime_set_autosuspend_delay(glue->dev, MUSB_AUTOSUSPEND_DELAY);
		pm_runtime_enable(glue->dev);
		pm_runtime_get_noresume(glue->dev);
		pm_runtime_mark_last_busy(glue->dev);
		pm_runtime_put_autosuspend(glue->dev);

		/* put controller and phy in suspend if no cable connected */
		if (!is_slave &&
			test_bit(ID, &glue->inputs) &&
			!test_bit(B_SESS_VLD, &glue->inputs)) {
			musb_sprd_detect_cable(glue);
			glue->drd_state = DRD_STATE_IDLE;
			break;
		}

		dev_dbg(glue->dev, "Exit UNDEF");
		glue->drd_state = DRD_STATE_IDLE;
		fallthrough;
	case DRD_STATE_IDLE:
		/* if /sys/class/usb_notify/usb_control/usb_data_enabled is
		 * 0, don't setup usb connection.
		 */
		if (test_bit(B_DATA_DISABLED, &glue->inputs)) {
			dev_dbg(glue->dev, "USB data disabled, wait\n");
			rework = true;
			delay = MUSB_DATA_ENABLE_CHECK_DELAY;
			break;
		}

		if (!test_bit(ID, &glue->inputs)) {
			dev_dbg(glue->dev, "!id\n");
			if (!pm_runtime_suspended(glue->dev)) {
				dev_info(glue->dev, "waiting glue suspended in host mode\n");
				rework = true;
				delay = MUSB_RUNTIME_CHECK_DELAY;
			} else {
				glue->drd_state = DRD_STATE_HOST_IDLE;
				rework = true;
			}
		} else if (test_bit(B_SESS_VLD, &glue->inputs)) {
			dev_dbg(glue->dev, "b_sess_vld\n");
			if (!musb_sprd_is_udc_start(glue)) {
				dev_info(glue->dev, "waiting musb udc start\n");
				rework = true;
				delay = MUSB_UDC_START_CHECK_DELAY;
			} else {
				/*
				 * Increment pm usage count upon cable connect. Count
				 * is decremented in DRD_STATE_PERIPHERAL state on
				 * cable disconnect or in bus suspend.
				 */
				pm_runtime_get_sync(glue->dev);
				/*switch dpdm to phy*/
				if (glue->use_pdhub_c2c)
					call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
						true, NULL);
				musb_sprd_otg_start_peripheral(glue, 1);
				glue->drd_state = DRD_STATE_PERIPHERAL;
				rework = true;
			}
		} else {
			dev_dbg(glue->dev, "Cable disconnected\n");
		}
		break;
	case DRD_STATE_PERIPHERAL:
		if (!test_bit(B_SESS_VLD, &glue->inputs) ||
				!test_bit(ID, &glue->inputs)) {
			dev_dbg(glue->dev, "!id || !bsv\n");
			glue->drd_state = DRD_STATE_RUNTIME_SUSPENDING;
			musb_sprd_otg_start_peripheral(glue, 0);
			/*
			 * Decrement pm usage count upon cable disconnect
			 * which was incremented upon cable connect in
			 * DRD_STATE_IDLE state
			 */
			pm_runtime_put_sync(glue->dev);
			rework = true;
		} else if (test_bit(B_DATA_DISABLED, &glue->inputs)) {
			dev_info(glue->dev, "usb data disabled\n");
			glue->drd_state = DRD_STATE_PERIPHERAL_DATA_DIS;
			musb_sprd_otg_start_peripheral(glue, 0);
			pm_runtime_put_sync(glue->dev);
			rework = true;
		} else if (test_bit(B_SUSPEND, &glue->inputs) &&
			test_bit(B_SESS_VLD, &glue->inputs)) {
			dev_dbg(glue->dev, "BPER bsv && susp\n");
			glue->drd_state = DRD_STATE_PERIPHERAL_SUSPEND;
			/*
			 * Decrement pm usage count upon bus suspend.
			 * Count was incremented either upon cable
			 * connect in DRD_STATE_IDLE or host
			 * initiated resume after bus suspend in
			 * DRD_STATE_PERIPHERAL_SUSPEND state
			 */
			pm_runtime_mark_last_busy(glue->dev);
			pm_runtime_put_autosuspend(glue->dev);
		}
		break;
	case DRD_STATE_PERIPHERAL_DATA_DIS:
		if (!test_bit(B_SESS_VLD, &glue->inputs) ||
			!test_bit(ID, &glue->inputs) ||
			!test_bit(B_DATA_DISABLED, &glue->inputs)) {
			dev_dbg(glue->dev, "!id || !bsv || !B_DATA_DISABLED\n");
			glue->drd_state = DRD_STATE_RUNTIME_SUSPENDING;
			rework = true;
		}
		break;
	case DRD_STATE_PERIPHERAL_SUSPEND:
		if (!test_bit(B_SESS_VLD, &glue->inputs) ||
				!test_bit(ID, &glue->inputs)) {
			dev_dbg(glue->dev, "BSUSP: !id || !bsv\n");
			glue->drd_state = DRD_STATE_RUNTIME_SUSPENDING;
			musb_sprd_otg_start_peripheral(glue, 0);
		} else if (!test_bit(B_SUSPEND, &glue->inputs)) {
			dev_dbg(glue->dev, "BSUSP !susp\n");
			glue->drd_state = DRD_STATE_PERIPHERAL;
			/*
			 * Increment pm usage count upon host
			 * initiated resume. Count was decremented
			 * upon bus suspend in
			 * DRD_STATE_PERIPHERAL state.
			 */
			pm_runtime_get_sync(glue->dev);
		}
		break;
	case DRD_STATE_HOST_IDLE:
		/* Switch to A-Device*/
		if (test_bit(ID, &glue->inputs)) {
			dev_dbg(glue->dev, "id\n");
			glue->drd_state = DRD_STATE_IDLE;
			glue->start_host_retry_count = 0;
			rework = true;
		} else {
			/*switch dpdm to phy*/
			if (glue->use_pdhub_c2c)
				call_sprd_usbphy_event_notifiers(SPRD_USBPHY_EVENT_TYPEC,
						true, NULL);
			ret = musb_sprd_otg_start_host(glue, 1);
			if ((ret == -EPROBE_DEFER) &&
						glue->start_host_retry_count < 3) {
				/*
				 * Get regulator failed as regulator driver is
				 * not up yet. Will try to start host after 1sec
				 */
				dev_dbg(glue->dev, "Unable to get vbus regulator. Retrying...\n");
				delay = VBUS_REG_CHECK_DELAY;
				rework = true;
				glue->start_host_retry_count++;
			} else if (ret) {
				/* do not need change drd_state */
				dev_err(glue->dev, "unable to start host\n");
			} else {
				glue->drd_state = DRD_STATE_HOST;
			}
		}
		break;
	case DRD_STATE_HOST:
		if (test_bit(ID, &glue->inputs)) {
			dev_dbg(glue->dev, "id\n");
			glue->drd_state = DRD_STATE_RUNTIME_SUSPENDING;
			musb_sprd_otg_start_host(glue, 0);
			glue->start_host_retry_count = 0;
			rework = true;
		} else if (test_bit(A_RECOVER, &glue->inputs)) {
			dev_dbg(glue->dev, "A Recover!\n");
			clear_bit(A_RECOVER, &glue->inputs);
			glue->drd_state = DRD_STATE_RUNTIME_SUSPENDING;
			musb_sprd_otg_start_host(glue, 0);
			glue->start_host_retry_count = 0;
			rework = true;
		}
		break;
	case DRD_STATE_RUNTIME_SUSPENDING:
		if (!pm_runtime_suspended(glue->dev)) {
			if (test_bit(B_SESS_VLD, &glue->inputs) && glue->use_pdhub_c2c) {
				pm_wakeup_event(glue->dev, 100);
				pm_runtime_get_sync(glue->musb->controller);
				sprd_musb_reset_controller(glue);
				pm_runtime_put_sync(glue->musb->controller);

				glue->musb->is_active = 0;
				glue->musb->xceiv->otg->default_a = 0;
				glue->musb->xceiv->otg->state = OTG_STATE_B_IDLE;
				MUSB_DEV_MODE(glue->musb);
				glue->dr_mode = USB_DR_MODE_UNKNOWN;
				glue->drd_state = DRD_STATE_IDLE;
				rework = true;
				dev_info(glue->dev, "skip DRD_STATE_RUNTIME_SUSPENDING\n");
				break;
			}

			dev_info(glue->dev, "waiting glue suspended\n");
			rework = true;
			delay = MUSB_RUNTIME_CHECK_DELAY;
		} else {
			glue->musb->is_active = 0;
			glue->musb->xceiv->otg->default_a = 0;
			glue->musb->xceiv->otg->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(glue->musb);
			glue->dr_mode = USB_DR_MODE_UNKNOWN;
			glue->drd_state = DRD_STATE_IDLE;
			rework = true;
		}
		break;
	default:
		dev_err(glue->dev, "%s: invalid otg-state\n", __func__);
		break;
	}

	if (rework)
		queue_delayed_work(glue->sm_usb_wq, &glue->sm_work, delay);

}

static int dwc3_sprd_driver_probe_finish(struct device *dev)
{
	while (!dwc3_sprd_probe_finish())
		return -EPROBE_DEFER;

	dev_info(dev, "dwc3 probe finish or donnot need loading, musb start probe\n");
	return 0;
}

static int musb_sprd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *cmdline_node;
	struct musb_hdrc_platform_data pdata;
	struct platform_device_info pinfo;
	struct sprd_glue *glue;
	const char *cmdline;
	u32 buf[2];
	int ret;
	int cali_maxspeed;
	bool mode;

	if (sprd_usbmux_check_mode() == MUX_MODE) {
		dev_info(&pdev->dev, "musb driver stop probe since usb mux jtag\n");
		return -ENODEV;
	}

	/* Workaround: force dwc3 and musb serialization when they are both exist */
	ret = dwc3_sprd_driver_probe_finish(&pdev->dev);
	if (ret)
		return ret;

	glue = devm_kzalloc(&pdev->dev, sizeof(*glue), GFP_KERNEL);
	if (!glue)
		return -ENOMEM;

	glue->dev = &pdev->dev;
	memset(&pdata, 0, sizeof(pdata));
	if (IS_ENABLED(CONFIG_USB_MUSB_GADGET))
		pdata.mode = MUSB_PERIPHERAL;
	else if (IS_ENABLED(CONFIG_USB_MUSB_HOST))
		pdata.mode = MUSB_HOST;
	else if (IS_ENABLED(CONFIG_USB_MUSB_DUAL_ROLE)) {
		enum usb_dr_mode mode = usb_get_dr_mode(dev);
		if (mode == USB_DR_MODE_HOST)
			pdata.mode = MUSB_HOST;
		else if (mode == USB_DR_MODE_PERIPHERAL)
			pdata.mode = MUSB_PERIPHERAL;
		else
			pdata.mode = MUSB_OTG;
	} else
		dev_err(&pdev->dev, "Invalid or missing 'dr_mode' property\n");

	dev_info(dev, "musb dr_mode: %d\n", pdata.mode);

	glue->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(glue->clk)) {
		dev_err(dev, "no core clk specified\n");
		return PTR_ERR(glue->clk);
	}
	ret = clk_prepare_enable(glue->clk);
	if (ret) {
		dev_err(dev, "clk_prepare_enable(glue->clk) failed\n");
		return ret;
	}

#if IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER)
	glue->hclk_src_sel = devm_clk_get(dev, "hclk_source_sel");
	if (IS_ERR(glue->hclk_src_sel)) {
		dev_warn(dev, "no hclk_source_sel specified\n");
		glue->hclk_src_sel = NULL;
	}

	glue->hclk_default_src = devm_clk_get(dev, "hclk_default_source");
	if (IS_ERR(glue->hclk_default_src)) {
		dev_warn(dev, "no hclk_default_source specified\n");
		glue->hclk_default_src = NULL;
	}

	glue->hclk_suspend_src = devm_clk_get(dev, "hclk_suspend_source");
	if (IS_ERR(glue->hclk_suspend_src)) {
		dev_warn(dev, "no hclk_suspend_source specified");
		glue->hclk_suspend_src = NULL;
	}
#endif
	glue->musb_wq = alloc_ordered_workqueue("musb_wq", 0);
	if (!glue->musb_wq) {
		clk_disable_unprepare(glue->clk);
		pr_err("%s: Unable to create workqueue musb_wq\n", __func__);
		return -ENOMEM;
	}

	/*
	 * Create an ordered freezable workqueue for sm_work so that it gets
	 * scheduled only after pm_resume has happened completely. This helps
	 * in avoiding race conditions between xhci_plat_resume and
	 * xhci_runtime_resume and also between hcd disconnect and xhci_resume.
	 */
	glue->sm_usb_wq = alloc_ordered_workqueue("k_sm_usb", WQ_FREEZABLE);
	if (!glue->sm_usb_wq) {
		clk_disable_unprepare(glue->clk);
		destroy_workqueue(glue->musb_wq);
		pr_err("%s: Unable to create workqueue k_sm_usb\n", __func__);
		return -ENOMEM;
	}

	glue->xceiv = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	if (IS_ERR(glue->xceiv)) {
		ret = PTR_ERR(glue->xceiv);
		dev_err(&pdev->dev, "Error getting usb-phy %d\n", ret);
		goto err_core_clk;
	}

	glue->pmu = syscon_regmap_lookup_by_phandle_args(dev->of_node,
							 "syscons", 2, buf);
	if (IS_ERR(glue->pmu)) {
		dev_warn(&pdev->dev, "failed to get pmu regmap!\n");
		glue->pmu = NULL;
	} else {
		glue->usb_pub_slp_poll_offset = buf[0];
		glue->usb_pub_slp_poll_mask = buf[1];
	}

	spin_lock_init(&glue->lock);

	INIT_WORK(&glue->resume_work, musb_sprd_resume_work);
	INIT_WORK(&glue->audio_work, musb_sprd_audio_work);
	INIT_DELAYED_WORK(&glue->sm_work, musb_sprd_otg_sm_work);
	INIT_DELAYED_WORK(&glue->chg_detect_work, musb_sprd_chg_detect_work);

	platform_set_drvdata(pdev, glue);

	pdata.platform_ops = &sprd_musb_ops;
	if (of_property_read_bool(node, "use-single-fifo")) {
		pdata.config = &sprd_musb_hdrc_config_single;
		glue->use_singlefifo = true;
		dev_info(&pdev->dev, "use single fifo\n");
	} else {
		pdata.config = &sprd_musb_hdrc_config;
		glue->use_singlefifo = false;
	}

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_err(&pdev->dev, "Can't not parse bootargs\n");
		goto err_core_clk;
	}

	mode = (strstr(cmdline, "androidboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "androidboot.mode=autotest") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=cali") != NULL) ||
	       (strstr(cmdline, "sprdboot.mode=autotest") != NULL);
	if (mode) {
		if (of_property_read_bool(node, "cali-fs")) {
			cali_maxspeed = USB_SPEED_FULL;
			dev_info(&pdev->dev, "cali maxspeed limits to fs\n");
		} else {
			cali_maxspeed = USB_SPEED_HIGH;
			dev_info(&pdev->dev, "cali maxspeed limits to hs\n");
		}
		if (glue->use_singlefifo)
			sprd_musb_hdrc_config_single.maximum_speed = cali_maxspeed;
		else
			sprd_musb_hdrc_config.maximum_speed = cali_maxspeed;
	}

	glue->enable_pm_suspend_in_host = of_property_read_bool(node, "wakeup-source");
	pdata.board_data = &glue->enable_pm_suspend_in_host;
	atomic_set(&glue->pm_suspended, 0);
	memset(&pinfo, 0, sizeof(pinfo));
	pinfo.name = "musb-hdrc";
	pinfo.id = PLATFORM_DEVID_AUTO;
	pinfo.parent = &pdev->dev;
	pinfo.res = pdev->resource;
	pinfo.num_res = pdev->num_resources;
	pinfo.data = &pdata;
	pinfo.size_data = sizeof(pdata);
	pinfo.dma_mask = DMA_BIT_MASK(BITS_PER_LONG);

	if (of_property_read_bool(node, "multipoint")) {
		if (glue->use_singlefifo)
			sprd_musb_hdrc_config_single.multipoint = true;
		else
			sprd_musb_hdrc_config.multipoint = true;
	} else
		dev_info(&pdev->dev, "not support multipoint\n");

	glue->musb_pdev = platform_device_register_full(&pinfo);
	if (IS_ERR(glue->musb_pdev)) {
		ret = PTR_ERR(glue->musb_pdev);
		dev_err(&pdev->dev, "Error registering musb dev: %d\n", ret);
		goto err_core_clk;
	}

	ret = of_property_read_u32_array(dev->of_node, "core_select",
			glue->musb->core_select, 2);
	if (ret) {
		dev_err(&pdev->dev, "failed to get core_select: %d\n", ret);
		glue->musb->performance_mode_rdy = false;
	} else
		glue->musb->performance_mode_rdy = true;

#if IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER)
	glue->suspend_clk_src_frc_on.regmap_ptr = syscon_regmap_lookup_by_phandle_args(dev->of_node,
						"suspend_clk_source_frc_on", 2,
						glue->suspend_clk_src_frc_on.args);
	if (IS_ERR(glue->suspend_clk_src_frc_on.regmap_ptr)) {
		dev_warn(&pdev->dev, "failed to get suspend_clk_source_frc_on regmap!\n");
		glue->suspend_clk_src_frc_on.regmap_ptr = NULL;
	}

	glue->pubsys_bypass.regmap_ptr = syscon_regmap_lookup_by_phandle_args(dev->of_node,
						"pubsys_bypass", 2, glue->pubsys_bypass.args);
	if (IS_ERR(glue->pubsys_bypass.regmap_ptr)) {
		dev_warn(&pdev->dev, "failed to get pubsys_bypass regmap!\n");
		glue->pubsys_bypass.regmap_ptr = NULL;
	}
#endif
	/*  GPIOs now */
	/* get vbus/id gpios extcon device */
	if (of_property_read_bool(node, "extcon")) {
		glue->edev = extcon_get_edev_by_phandle(glue->dev, 0);
		if (IS_ERR(glue->edev)) {
			ret = PTR_ERR(glue->edev);
			dev_err(dev, "failed to find vbus extcon device.\n");
			goto err_glue_musb;
		}
		glue->vbus_nb.notifier_call = musb_sprd_vbus_notifier;
		ret = extcon_register_notifier(glue->edev, EXTCON_USB,
						&glue->vbus_nb);
		if (ret) {
			dev_err(dev,
				"failed to register extcon USB notifier.\n");
			goto err_glue_musb;
		}

		glue->id_edev = extcon_get_edev_by_phandle(glue->dev, 1);
		if (IS_ERR(glue->id_edev)) {
			glue->id_edev = NULL;
			dev_info(dev, "No separate ID extcon device.\n");
		}

		glue->id_nb.notifier_call = musb_sprd_id_notifier;
		if (glue->id_edev)
			ret = extcon_register_notifier(glue->id_edev,
						       EXTCON_USB_HOST,
						       &glue->id_nb);
		else
			ret = extcon_register_notifier(glue->edev,
						       EXTCON_USB_HOST,
						       &glue->id_nb);
		if (ret) {
			dev_err(dev,
			"failed to register extcon USB HOST notifier.\n");
			goto err_extcon_vbus;
		}

		glue->audio_high_speed_nb.notifier_call = musb_sprd_audio_high_speed_notifier;
		ret = register_sprd_usbm_notifier(&glue->audio_high_speed_nb,
			SPRD_USBM_EVENT_AUDIO_HIGH_SPEED);
		if (ret) {
			dev_err(glue->dev, "failed to register audio high speed event\n");
			goto err_extcon_vbus;
		}

		if (pdata.mode == MUSB_HOST && !is_slave)
			glue->id_state = MUSB_ID_GROUND;
		else
			glue->id_state = MUSB_ID_FLOAT;
	} else {
		struct musb *musb = glue->musb;

		switch (musb->port_mode) {
		case MUSB_HOST:
			glue->id_state = MUSB_ID_GROUND;
			break;
		case MUSB_PERIPHERAL:
		case MUSB_OTG:
			glue->id_state = MUSB_ID_FLOAT;
			glue->vbus_active = true;
			break;
		default:
			dev_err(dev, "unsupported port mode\n");
			break;
		}
	}

	glue->usb_data_enabled = 1;

	if (!is_slave) {
		ret = musb_sprd_usb_notify_init(pdev, glue);
		if (ret)
			dev_err(glue->dev, "usb_notify_init err %d\n", ret);
	}

	glue->audio_nb.notifier_call = musb_sprd_audio_notifier;
	ret = register_sprd_usbm_notifier(&glue->audio_nb, SPRD_USBM_EVENT_HOST_MUSB);
	if (ret) {
		dev_err(glue->dev, "failed to register usb event\n");
		goto err_extcon_vbus;
	}

	glue->wake_lock = wakeup_source_create("musb-sprd");
	wakeup_source_add(glue->wake_lock);
	glue->pd_wake_lock = wakeup_source_create("musb-sprd-pd");
	wakeup_source_add(glue->pd_wake_lock);
	glue->use_pdhub_c2c = of_property_read_bool(node, "use_pdhub_c2c");

	timer_setup(&glue->relax_wakelock_timer, sprd_relax_wakelock_timer, 0);
	glue->wake_lock_relaxed = true;

	if (of_device_is_compatible(node, "sprd,sharkl5pro-musb")) {
		struct musb *musb = glue->musb;

		dev_info(glue->dev, "set fixup_ep0fifo for L5Pro\n");
		musb->fixup_ep0fifo = 1;
	}

	ret = sysfs_create_groups(&glue->dev->kobj, musb_sprd_groups);
	if (ret)
		dev_warn(glue->dev, "failed to create musb attributes\n");

	musb_sprd_charger_mode();
	atomic_set(&glue->musb_runtime_suspended, 0);

	musb_sprd_ext_event_notify(glue);

	return 0;

err_extcon_vbus:
	if (glue->edev)
		extcon_unregister_notifier(glue->edev, EXTCON_USB,
					&glue->vbus_nb);

err_glue_musb:
	platform_device_unregister(glue->musb_pdev);

err_core_clk:
	clk_disable_unprepare(glue->clk);

	destroy_workqueue(glue->musb_wq);
	destroy_workqueue(glue->sm_usb_wq);
	return ret;
}

static int musb_sprd_remove(struct platform_device *pdev)
{
	struct sprd_glue *glue = platform_get_drvdata(pdev);
	struct musb *musb = glue->musb;

	musb_sprd_usb_notify_exit(pdev);
	/* this gets called on rmmod.
	 *  - Host mode: host may still be active
	 *  - Peripheral mode: peripheral is deactivated (or never-activated)
	 *  - OTG mode: both roles are deactivated (or never-activated)
	 */
	if (musb->dma_controller)
		musb_dma_controller_destroy(musb->dma_controller);
	musb->dma_controller = NULL;
	sysfs_remove_groups(&glue->dev->kobj, musb_sprd_groups);

	cancel_delayed_work_sync(&musb->irq_work);
	cancel_delayed_work_sync(&musb->finish_resume_work);
	cancel_delayed_work_sync(&musb->deassert_reset_work);

	usb_role_switch_unregister(glue->role_sw);
	platform_device_unregister(glue->musb_pdev);

	destroy_workqueue(glue->musb_wq);
	destroy_workqueue(glue->sm_usb_wq);
	return 0;
}

static void musb_sprd_release_all_request(struct musb *musb)
{
	struct musb_ep *musb_ep_in;
	struct musb_ep *musb_ep_out;
	struct musb_hw_ep *endpoints;
	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	u32 i;

	for (i = 1; i < musb->config->num_eps; i++) {
		endpoints = &musb->endpoints[i];
		if (!endpoints)
			continue;
		musb_ep_in = &endpoints->ep_in;
		if (musb_ep_in && musb_ep_in->dma) {
			ep_in = &musb_ep_in->end_point;
			usb_ep_disable(ep_in);
		}
		musb_ep_out = &endpoints->ep_out;
		if (musb_ep_out && musb_ep_out->dma) {
			ep_out = &musb_ep_out->end_point;
			usb_ep_disable(ep_out);
		}
	}
}

static void musb_sprd_disable_all_interrupts(struct musb *musb)
{
	void __iomem	*mbase = musb->mregs;
	u16	temp;
	u32	i;
	u32	intr;

	/* disable interrupts */
	musb_writeb(mbase, MUSB_INTRUSBE, 0);
	musb_writew(mbase, MUSB_INTRTXE, 0);
	musb_writew(mbase, MUSB_INTRRXE, 0);

	/*  flush pending interrupts */
	temp = musb_readb(mbase, MUSB_INTRUSB);
	temp = musb_readw(mbase, MUSB_INTRTX);
	temp = musb_readw(mbase, MUSB_INTRRX);

	/* disable dma interrupts */
	for (i = 1; i <= MUSB_DMA_CHANNELS; i++) {
		intr = musb_readl(mbase, MUSB_DMA_CHN_INTR(i));
		intr &= ~(CHN_LLIST_INT_EN | CHN_START_INT_EN |
			CHN_USBRX_INT_EN | CHN_CLEAR_INT_EN);
		musb_writel(mbase, MUSB_DMA_CHN_INTR(i),
			intr);
	}

	/* flush dma interrupts */
	for (i = 1; i <= MUSB_DMA_CHANNELS; i++) {
		intr = musb_readl(mbase, MUSB_DMA_CHN_INTR(i));
		intr |= CHN_LLIST_INT_CLR | CHN_START_INT_CLR |
			CHN_FRAG_INT_CLR | CHN_BLK_INT_CLR |
			CHN_USBRX_LAST_INT_CLR;
		musb_writel(mbase, MUSB_DMA_CHN_INTR(i),
			intr);
	}
}

static int musb_sprd_pm_suspend(struct device *dev)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);
	struct musb *musb = glue->musb;
	u32 msk, val;

	dev_info(glue->dev, "%s: enter\n", __func__);

#if IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER)
	if (musb_sprd_lowpower_configuration_onoff(glue, 1))
		return 0;
#else
	if (glue->vbus_active && glue->dr_mode == USB_DR_MODE_PERIPHERAL) {
		dev_info(glue->dev, "Abort PM suspend in device mode!!\n");
		return -EBUSY;
	}

	if (!glue->enable_pm_suspend_in_host && glue->dr_mode == USB_DR_MODE_HOST) {
		dev_info(glue->dev, "Abort PM suspend in host mode!!\n");
		return -EBUSY;
	}
#endif
	if (musb->is_offload && !musb->offload_used) {
		if (glue->pmu) {
			val = msk = glue->usb_pub_slp_poll_mask;
			regmap_update_bits(glue->pmu,
					   glue->usb_pub_slp_poll_offset,
					   msk, val);
		}
	}

	/* in host mode, don't do suspend */
	if (glue->dr_mode == USB_DR_MODE_HOST) {
		dev_info(glue->dev, "don't do %s in host mode\n", __func__);
		return 0;
	}

	musb_sprd_suspend(glue);
	atomic_set(&glue->pm_suspended, 1);

	return 0;
}

static int musb_sprd_pm_resume(struct device *dev)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);
	struct musb *musb = glue->musb;
	u32 msk;

	dev_info(glue->dev, "%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_MUSB_SPRD_LOWPOWER)
	if (musb_sprd_lowpower_configuration_onoff(glue, 0))
		return 0;
#endif
	if (musb->is_offload && !musb->offload_used) {
		if (glue->pmu) {
			msk = glue->usb_pub_slp_poll_mask;
			regmap_update_bits(glue->pmu,
					   glue->usb_pub_slp_poll_offset,
					   msk, 0);
		}
	}

	if (!atomic_read(&glue->pm_suspended)) {
		dev_info(glue->dev, "musb sprd pm is not suspended\n");
		return 0;
	}

	musb_sprd_resume(glue);
	atomic_set(&glue->pm_suspended, 0);

	/* Reset and enable sprd musb PM runtime here,
	 * clk, phy are already resumed in musb_sprd_resume,
	 * so here just increase runtime PM usage count
	 */
	pm_runtime_disable(glue->dev);
	pm_runtime_set_autosuspend_delay(glue->dev, MUSB_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(glue->dev);
	pm_runtime_get_noresume(glue->dev);
	pm_runtime_set_active(glue->dev);
	pm_runtime_enable(glue->dev);
	pm_runtime_mark_last_busy(glue->dev);
	pm_runtime_put_autosuspend(glue->dev);

	/* kick in otg state machine */
	queue_work(glue->musb_wq, &glue->resume_work);
	return 0;
}

static int musb_sprd_runtime_suspend(struct device *dev)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);

	dev_info(glue->dev, "%s: enter\n", __func__);
	musb_sprd_suspend(glue);
	return 0;
}

static int musb_sprd_runtime_resume(struct device *dev)
{
	struct sprd_glue *glue = dev_get_drvdata(dev);

	dev_info(glue->dev, "%s: enter\n", __func__);
	musb_sprd_resume(glue);
	return 0;
}

static int musb_sprd_runtime_idle(struct device *dev)
{
	dev_info(dev, "enter into idle mode\n");
	return 0;
}

static const struct dev_pm_ops musb_sprd_pm_ops = {
	.suspend	= musb_sprd_pm_suspend,
	.resume		= musb_sprd_pm_resume,
	.runtime_suspend = musb_sprd_runtime_suspend,
	.runtime_resume = musb_sprd_runtime_resume,
	.runtime_idle = musb_sprd_runtime_idle,
};

static const struct of_device_id usb_ids[] = {
	{ .compatible = "sprd,pike2-musb" },
	{ .compatible = "sprd,sharkl3-musb" },
	{ .compatible = "sprd,sharkl5-musb" },
	{ .compatible = "sprd,sharkl5pro-musb" },
	{ .compatible = "sprd,qogirl6-musb" },
	{ .compatible = "sprd,qogirn6pro-musb" },
	{ .compatible = "sprd,qogirn6lite-musb" },
	{}
};

MODULE_DEVICE_TABLE(of, usb_ids);

static struct platform_driver musb_sprd_driver = {
	.driver = {
		.name = "musb-sprd",
		.pm = &musb_sprd_pm_ops,
		.of_match_table = usb_ids,
	},
	.probe = musb_sprd_probe,
	.remove = musb_sprd_remove,
};

static int __init musb_sprd_driver_init(void)
{
	return platform_driver_register(&musb_sprd_driver);
}

static void __exit musb_sprd_driver_exit(void)
{
	platform_driver_unregister(&musb_sprd_driver);
}

late_initcall(musb_sprd_driver_init);
module_exit(musb_sprd_driver_exit);

MODULE_DESCRIPTION(DRIVER_INFO);
MODULE_LICENSE("GPL v2");
