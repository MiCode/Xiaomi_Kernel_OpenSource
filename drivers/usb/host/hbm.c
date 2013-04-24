/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/usb/hbm.h>
#include <mach/usb_bam.h>

/**
 *  USB HBM Hardware registers.
 *
 */
#define USB_OTG_HS_HBM_CFG			(0x00000290)
#define USB_OTG_HS_HBM_QH_MAP_PIPE(n)		(0x00000294 + 4 * (n))
#define USB_OTG_HS_HBM_PIPE_PRODUCER		(0x00000314)
#define USB_OTG_HS_HBM_PARK_MODE_DISABLE	(0x00000318)
#define USB_OTG_HS_HBM_PIPE_ZLT_DISABLE		(0x0000031C)
#define USB_OTG_HS_HBM_PIPE_EN			(0x00000310)
#define USB_OTG_HS_HBM_SW_RST			(0x00000324)
#define USB_OTG_HS_HBM_SB_SW_RST		(0x00000320)
#define USB_OTG_HS_USBCMD			(0x00000140)
#define USB_OTG_HS_USBSTS			(0x00000144)

/**
 *  USB HBM  Hardware registers bitmask.
 */
#define HBM_EN		0x00000001
#define ASE		0x20
#define AS		0x8000
#define PIPE_PRODUCER	1
#define MAX_PIPE_NUM	16
#define HBM_QH_MAP_PIPE	0xffffffc0
#define QTD_CERR_MASK	0xfffff3ff

struct hbm_msm {
	u32 *base;
	struct usb_hcd *hcd;
	bool disable_park_mode;
};

static struct hbm_msm *hbm_ctx;

/**
 * Read register masked field.
 *
 * @base - hbm base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 *
 * @return u32
 */
static inline u32 hbm_msm_read_reg_field(void *base,
		u32 offset, const u32 mask)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 val = ioread32(base + offset);
	val &= mask;		/* clear other bits */
	val >>= shift;
	return val;
}

/**
 * Write register field.
 *
 * @base - hbm base virtual address.
 * @offset - register offset.
 * @val - value to be written.
 *
 */
static inline void hbm_msm_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

/**
 * Write register masked field.
 *
 * @base - hbm base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void hbm_msm_write_reg_field(void *base, u32 offset,
	const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;
	val = tmp | (val << shift);
	iowrite32(val, base + offset);
}

/**
 * Enable/disable park mode. Park mode enables executing up to 3 usb packets
 * from each QH.
 *
 * @pipe_num - Connection index.
 *
 * @disable_park_mode - Enable/disable park mode.
 *
 */
int set_disable_park_mode(u8 pipe_num, bool disable_park_mode)
{
	if (pipe_num >= MAX_PIPE_NUM) {
		pr_err("%s: illegal pipe num %d", __func__, pipe_num);
		return -EINVAL;
	}

	/*  enable/disable park mode */
	hbm_msm_write_reg_field(hbm_ctx->base,
		USB_OTG_HS_HBM_PARK_MODE_DISABLE, 1 << pipe_num,
		(disable_park_mode ? 1 : 0));
	return 0;
}

/**
 * Enable/disable zero length transfer.
 *
 * @pipe_num - Connection index.
 *
 * @disable_zlt - Enable/disable zlt.
 *
 */
int set_disable_zlt(u8 pipe_num, bool disable_zlt)
{
	if (pipe_num >= MAX_PIPE_NUM) {
		pr_err("%s: illegal pipe num %d", __func__, pipe_num);
		return -EINVAL;
	}

	/*  enable/disable zlt */
	hbm_msm_write_reg_field(hbm_ctx->base,
		USB_OTG_HS_HBM_PIPE_ZLT_DISABLE, 1 << pipe_num,
		(disable_zlt ? 1 : 0));
	return 0;
}

static void hbm_reset(bool reset)
{
	hbm_msm_write_reg_field(hbm_ctx->base, USB_OTG_HS_HBM_SW_RST, 1 << 0,
			reset ? 1 : 0);
}

static void hbm_config(bool enable)
{
	hbm_msm_write_reg_field(hbm_ctx->base, USB_OTG_HS_HBM_CFG, HBM_EN,
		enable ? 1 : 0);
}

int hbm_pipe_init(u32 QH_addr, u32 pipe_num, bool is_consumer)
{
	if (pipe_num >= MAX_PIPE_NUM) {
		pr_err("%s: illegal pipe num %d", __func__, pipe_num);
		return -EINVAL;
	}

	/* map QH(ep) <> pipe */
	hbm_msm_write_reg(hbm_ctx->base,
		USB_OTG_HS_HBM_QH_MAP_PIPE(pipe_num), QH_addr);

	/* set pipe producer/consumer mode - (IN EP is  producer) */
	hbm_msm_write_reg_field(hbm_ctx->base,
		USB_OTG_HS_HBM_PIPE_PRODUCER, 1 << pipe_num,
		(is_consumer ? 0 : 1));

	/*  set park mode */
	set_disable_park_mode(pipe_num, hbm_ctx->disable_park_mode);

	/*  enable zlt as default*/
	set_disable_zlt(pipe_num, false);

	/* activate pipe */
	hbm_msm_write_reg_field(hbm_ctx->base, USB_OTG_HS_HBM_PIPE_EN,
		1 << pipe_num, 1);

	return 0;
}

void hbm_init(struct usb_hcd *hcd, bool disable_park_mode)
{
	pr_info("%s\n", __func__);

	hbm_ctx = kzalloc(sizeof(*hbm_ctx), GFP_KERNEL);
	if (!hbm_ctx) {
		pr_err("%s: hbm_ctx alloc failed\n", __func__);
		return;
	}

	hbm_ctx->base = hcd->regs;
	hbm_ctx->hcd = hcd;
	hbm_ctx->disable_park_mode = disable_park_mode;

	/* reset hbm */
	hbm_reset(true);
	/* delay was added to allow the reset process the end */
	udelay(1000);
	hbm_reset(false);
	hbm_config(true);
}

void hbm_uninit(void)
{
	hbm_config(false);
	kfree(hbm_ctx);
}

static int hbm_submit_async(struct ehci_hcd *ehci, struct urb *urb,
	struct list_head *qtd_list, gfp_t mem_flags)
{
	int epnum;
	unsigned long flags;
	struct ehci_qh *qh = NULL;
	int rc;
	struct usb_host_bam_type *bam =
		(struct usb_host_bam_type *)urb->priv_data;

	epnum = urb->ep->desc.bEndpointAddress;

	spin_lock_irqsave(&ehci->lock, flags);
	if (unlikely(!HCD_HW_ACCESSIBLE(ehci_to_hcd(ehci)))) {
		rc = -ESHUTDOWN;
		goto done;
	}
	rc = usb_hcd_link_urb_to_ep(ehci_to_hcd(ehci), urb);
	if (unlikely(rc))
		goto done;

	qh = qh_append_tds(ehci, urb, qtd_list, epnum, &urb->ep->hcpriv);
	if (unlikely(qh == NULL)) {
		usb_hcd_unlink_urb_from_ep(ehci_to_hcd(ehci), urb);
		rc = -ENOMEM;
		goto done;
	}

	hbm_pipe_init(qh->qh_dma, bam->pipe_num, bam->dir);

	if (likely(qh->qh_state == QH_STATE_IDLE))
		qh_link_async(ehci, qh);

done:
	spin_unlock_irqrestore(&ehci->lock, flags);
	if (unlikely(qh == NULL))
		qtd_list_free(ehci, urb, qtd_list);
	return rc;
}

int hbm_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
			gfp_t mem_flags)
{
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	struct list_head qtd_list;
	struct ehci_qtd *qtd;

	INIT_LIST_HEAD(&qtd_list);

	if (usb_pipetype(urb->pipe) != PIPE_BULK) {
		pr_err("%s pipe type is not BULK\n", __func__);
		return -EINVAL;
	}

	/*no sg support*/
	urb->transfer_buffer_length = 0;
	urb->transfer_dma = 0;
	urb->transfer_flags |= URB_NO_INTERRUPT;

	if (!qh_urb_transaction(ehci, urb, &qtd_list, mem_flags))
		return -ENOMEM;

	/* set err counter in qTD token to zero */
	qtd = list_entry(qtd_list.next, struct ehci_qtd, qtd_list);
	if (qtd != NULL)
		qtd->hw_token &= QTD_CERR_MASK;

	return hbm_submit_async(ehci, urb, &qtd_list, mem_flags);
}
