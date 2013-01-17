/*
 * Driver for HighSpeed USB Client Controller in MSM7K
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 * Author: Mike Lockwood <lockwood@android.com>
 *         Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/pm_runtime.h>

#include <mach/msm72k_otg.h>
#include <linux/io.h>

#include <asm/mach-types.h>

#include <mach/board.h>
#include <mach/msm_hsusb.h>
#include <linux/device.h>
#include <mach/msm_hsusb_hw.h>
#include <mach/clk.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>

static const char driver_name[] = "msm72k_udc";

/* #define DEBUG */
/* #define VERBOSE */

#define MSM_USB_BASE ((unsigned) ui->addr)

#define	DRIVER_DESC		"MSM 72K USB Peripheral Controller"
#define	DRIVER_NAME		"MSM72K_UDC"

#define EPT_FLAG_IN        0x0001

#define SETUP_BUF_SIZE     8


static const char *const ep_name[] = {
	"ep0out", "ep1out", "ep2out", "ep3out",
	"ep4out", "ep5out", "ep6out", "ep7out",
	"ep8out", "ep9out", "ep10out", "ep11out",
	"ep12out", "ep13out", "ep14out", "ep15out",
	"ep0in", "ep1in", "ep2in", "ep3in",
	"ep4in", "ep5in", "ep6in", "ep7in",
	"ep8in", "ep9in", "ep10in", "ep11in",
	"ep12in", "ep13in", "ep14in", "ep15in"
};

/*To release the wakelock from debugfs*/
static int release_wlocks;

struct msm_request {
	struct usb_request req;

	/* saved copy of req.complete */
	void	(*gadget_complete)(struct usb_ep *ep,
					struct usb_request *req);


	struct usb_info *ui;
	struct msm_request *next;
	struct msm_request *prev;

	unsigned busy:1;
	unsigned live:1;
	unsigned alloced:1;

	dma_addr_t dma;
	dma_addr_t item_dma;

	struct ept_queue_item *item;
};

#define to_msm_request(r) container_of(r, struct msm_request, req)
#define to_msm_endpoint(r) container_of(r, struct msm_endpoint, ep)
#define to_msm_otg(xceiv)  container_of(xceiv, struct msm_otg, phy)
#define is_b_sess_vld()	((OTGSC_BSV & readl(USB_OTGSC)) ? 1 : 0)
#define is_usb_online(ui) (ui->usb_state != USB_STATE_NOTATTACHED)

struct msm_endpoint {
	struct usb_ep ep;
	struct usb_info *ui;
	struct msm_request *req; /* head of pending requests */
	struct msm_request *last;
	unsigned flags;

	/* bit number (0-31) in various status registers
	** as well as the index into the usb_info's array
	** of all endpoints
	*/
	unsigned char bit;
	unsigned char num;
	unsigned long dTD_update_fail_count;
	unsigned long false_prime_fail_count;
	unsigned actual_prime_fail_count;
	unsigned long dTD_workaround_fail_count;

	unsigned wedged:1;
	/* pointers to DMA transfer list area */
	/* these are allocated from the usb_info dma space */
	struct ept_queue_head *head;
	struct timer_list prime_timer;
};

/* PHY status check timer to monitor phy stuck up on reset */
static struct timer_list phy_status_timer;

static void ept_prime_timer_func(unsigned long data);
static void usb_do_work(struct work_struct *w);
static void usb_do_remote_wakeup(struct work_struct *w);


#define USB_STATE_IDLE    0
#define USB_STATE_ONLINE  1
#define USB_STATE_OFFLINE 2

#define USB_FLAG_START          0x0001
#define USB_FLAG_VBUS_ONLINE    0x0002
#define USB_FLAG_VBUS_OFFLINE   0x0004
#define USB_FLAG_RESET          0x0008
#define USB_FLAG_SUSPEND        0x0010
#define USB_FLAG_CONFIGURED     0x0020

#define USB_CHG_DET_DELAY	msecs_to_jiffies(1000)
#define REMOTE_WAKEUP_DELAY	msecs_to_jiffies(1000)
#define PHY_STATUS_CHECK_DELAY	(jiffies + msecs_to_jiffies(1000))
#define EPT_PRIME_CHECK_DELAY	(jiffies + msecs_to_jiffies(1000))

struct usb_info {
	/* lock for register/queue/device state changes */
	spinlock_t lock;

	/* single request used for handling setup transactions */
	struct usb_request *setup_req;

	struct platform_device *pdev;
	int irq;
	void *addr;

	unsigned state;
	unsigned flags;

	atomic_t configured;
	atomic_t running;

	struct dma_pool *pool;

	/* dma page to back the queue heads and items */
	unsigned char *buf;
	dma_addr_t dma;

	struct ept_queue_head *head;

	/* used for allocation */
	unsigned next_item;
	unsigned next_ifc_num;

	/* endpoints are ordered based on their status bits,
	** so they are OUT0, OUT1, ... OUT15, IN0, IN1, ... IN15
	*/
	struct msm_endpoint ept[32];


	/* max power requested by selected configuration */
	unsigned b_max_pow;
	unsigned chg_current;
	unsigned chg_type_retry_cnt;
	bool proprietary_chg;
	struct delayed_work chg_det;
	struct delayed_work chg_stop;
	struct msm_hsusb_gadget_platform_data *pdata;
	struct work_struct phy_status_check;

	struct work_struct work;
	unsigned phy_status;
	unsigned phy_fail_count;
	unsigned prime_fail_count;
	unsigned long dTD_update_fail_count;
	unsigned long dTD_workaround_fail_count;

	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	struct switch_dev sdev;

#define ep0out ept[0]
#define ep0in  ept[16]

	atomic_t ep0_dir;
	atomic_t test_mode;
	atomic_t offline_pending;
	atomic_t softconnect;
#ifdef CONFIG_USB_OTG
	u8 hnp_avail;
#endif

	atomic_t remote_wakeup;
	atomic_t self_powered;
	struct delayed_work rw_work;

	struct usb_phy *xceiv;
	enum usb_device_state usb_state;
	struct wake_lock	wlock;
};

static const struct usb_ep_ops msm72k_ep_ops;
static struct usb_info *the_usb_info;

static int msm72k_wakeup(struct usb_gadget *_gadget);
static int msm72k_pullup_internal(struct usb_gadget *_gadget, int is_active);
static int msm72k_set_halt(struct usb_ep *_ep, int value);
static void flush_endpoint(struct msm_endpoint *ept);
static void usb_reset(struct usb_info *ui);
static int usb_ept_set_halt(struct usb_ep *_ep, int value);

static void msm_hsusb_set_speed(struct usb_info *ui)
{
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	switch (readl(USB_PORTSC) & PORTSC_PSPD_MASK) {
	case PORTSC_PSPD_FS:
		dev_dbg(&ui->pdev->dev, "portchange USB_SPEED_FULL\n");
		ui->gadget.speed = USB_SPEED_FULL;
		break;
	case PORTSC_PSPD_LS:
		dev_dbg(&ui->pdev->dev, "portchange USB_SPEED_LOW\n");
		ui->gadget.speed = USB_SPEED_LOW;
		break;
	case PORTSC_PSPD_HS:
		dev_dbg(&ui->pdev->dev, "portchange USB_SPEED_HIGH\n");
		ui->gadget.speed = USB_SPEED_HIGH;
		break;
	}
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void msm_hsusb_set_state(enum usb_device_state state)
{
	unsigned long flags;

	spin_lock_irqsave(&the_usb_info->lock, flags);
	the_usb_info->usb_state = state;
	spin_unlock_irqrestore(&the_usb_info->lock, flags);
}

static enum usb_device_state msm_hsusb_get_state(void)
{
	unsigned long flags;
	enum usb_device_state state;

	spin_lock_irqsave(&the_usb_info->lock, flags);
	state = the_usb_info->usb_state;
	spin_unlock_irqrestore(&the_usb_info->lock, flags);

	return state;
}

static ssize_t print_switch_name(struct switch_dev *sdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", DRIVER_NAME);
}

static ssize_t print_switch_state(struct switch_dev *sdev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
		sdev->state ? "online" : "offline");
}

static inline enum chg_type usb_get_chg_type(struct usb_info *ui)
{
	if ((readl_relaxed(USB_PORTSC) & PORTSC_LS) == PORTSC_LS) {
		return USB_CHG_TYPE__WALLCHARGER;
	} else if (ui->pdata->prop_chg) {
		if (ui->gadget.speed == USB_SPEED_LOW ||
			ui->gadget.speed == USB_SPEED_FULL ||
			ui->gadget.speed == USB_SPEED_HIGH)
			return USB_CHG_TYPE__SDP;
		else
			return USB_CHG_TYPE__INVALID;
	} else {
		return USB_CHG_TYPE__SDP;
	}
}

#define USB_WALLCHARGER_CHG_CURRENT 1800
#define USB_PROPRIETARY_CHG_CURRENT 500
static int usb_get_max_power(struct usb_info *ui)
{
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	unsigned long flags;
	enum chg_type temp;
	int suspended;
	int configured;
	unsigned bmaxpow;

	if (ui->gadget.is_a_peripheral)
		return -EINVAL;

	temp = atomic_read(&otg->chg_type);
	spin_lock_irqsave(&ui->lock, flags);
	suspended = ui->usb_state == USB_STATE_SUSPENDED ? 1 : 0;
	configured = atomic_read(&ui->configured);
	bmaxpow = ui->b_max_pow;
	spin_unlock_irqrestore(&ui->lock, flags);

	if (temp == USB_CHG_TYPE__INVALID)
		return -ENODEV;

	if (temp == USB_CHG_TYPE__WALLCHARGER && !ui->proprietary_chg)
		return USB_WALLCHARGER_CHG_CURRENT;
	else if (ui->pdata->prop_chg)
		return USB_PROPRIETARY_CHG_CURRENT;

	if (suspended || !configured)
		return 0;

	return bmaxpow;
}

static int usb_phy_stuck_check(struct usb_info *ui)
{
	/*
	 * write some value (0xAA) into scratch reg (0x16) and read it back,
	 * If the read value is same as written value, means PHY is normal
	 * otherwise, PHY seems to have stuck.
	 */

	if (usb_phy_io_write(ui->xceiv, 0xAA, 0x16) == -1) {
		dev_dbg(&ui->pdev->dev,
				"%s(): ulpi write timeout\n", __func__);
		return -EIO;
	}

	if (usb_phy_io_read(ui->xceiv, 0x16) != 0xAA) {
		dev_dbg(&ui->pdev->dev,
				"%s(): read value is incorrect\n", __func__);
		return -EIO;
	}

	return 0;
}

/*
 * This function checks the phy status by reading/writing to the
 * phy scratch register. If the phy is stuck resets the HW
 * */
static void usb_phy_stuck_recover(struct work_struct *w)
{
	struct usb_info *ui = the_usb_info;
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->gadget.speed != USB_SPEED_UNKNOWN ||
			ui->usb_state == USB_STATE_NOTATTACHED ||
			ui->driver == NULL) {
		spin_unlock_irqrestore(&ui->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	disable_irq(otg->irq);
	if (usb_phy_stuck_check(ui)) {
#ifdef CONFIG_USB_MSM_ACA
		del_timer_sync(&otg->id_timer);
#endif
		ui->phy_fail_count++;
		dev_err(&ui->pdev->dev,
				"%s():PHY stuck, resetting HW\n", __func__);
		/*
		 * PHY seems to have stuck,
		 * reset the PHY and HW link to recover the PHY
		 */
		usb_reset(ui);
#ifdef CONFIG_USB_MSM_ACA
		mod_timer(&otg->id_timer, jiffies +
				 msecs_to_jiffies(OTG_ID_POLL_MS));
#endif
		msm72k_pullup_internal(&ui->gadget, 1);
	}
	enable_irq(otg->irq);
}

static void usb_phy_status_check_timer(unsigned long data)
{
	struct usb_info *ui = the_usb_info;

	schedule_work(&ui->phy_status_check);
}

static void usb_chg_stop(struct work_struct *w)
{
	struct usb_info *ui = container_of(w, struct usb_info, chg_stop.work);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	enum chg_type temp;

	temp = atomic_read(&otg->chg_type);

	if (temp == USB_CHG_TYPE__SDP)
		usb_phy_set_power(ui->xceiv, 0);
}

static void usb_chg_detect(struct work_struct *w)
{
	struct usb_info *ui = container_of(w, struct usb_info, chg_det.work);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	enum chg_type temp = USB_CHG_TYPE__INVALID;
	unsigned long flags;
	int maxpower;

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->usb_state == USB_STATE_NOTATTACHED) {
		spin_unlock_irqrestore(&ui->lock, flags);
		return;
	}

	temp = usb_get_chg_type(ui);
	if (temp != USB_CHG_TYPE__WALLCHARGER && temp != USB_CHG_TYPE__SDP
					&& !ui->chg_type_retry_cnt) {
		schedule_delayed_work(&ui->chg_det, USB_CHG_DET_DELAY);
		ui->chg_type_retry_cnt++;
		spin_unlock_irqrestore(&ui->lock, flags);
		return;
	}
	if (temp == USB_CHG_TYPE__INVALID) {
		temp = USB_CHG_TYPE__WALLCHARGER;
		ui->proprietary_chg = true;
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	atomic_set(&otg->chg_type, temp);
	maxpower = usb_get_max_power(ui);
	if (maxpower > 0)
		usb_phy_set_power(ui->xceiv, maxpower);

	/* USB driver prevents idle and suspend power collapse(pc)
	 * while USB cable is connected. But when dedicated charger is
	 * connected, driver can vote for idle and suspend pc.
	 * OTG driver handles idle pc as part of above usb_phy_set_power call
	 * when wallcharger is attached. To allow suspend pc, release the
	 * wakelock which will be re-acquired for any sub-sequent usb interrupts
	 * */
	if (temp == USB_CHG_TYPE__WALLCHARGER) {
		pm_runtime_put_sync(&ui->pdev->dev);
		wake_unlock(&ui->wlock);
	}
}

static int usb_ep_get_stall(struct msm_endpoint *ept)
{
	unsigned int n;
	struct usb_info *ui = ept->ui;

	n = readl(USB_ENDPTCTRL(ept->num));
	if (ept->flags & EPT_FLAG_IN)
		return (CTRL_TXS & n) ? 1 : 0;
	else
		return (CTRL_RXS & n) ? 1 : 0;
}

static void init_endpoints(struct usb_info *ui)
{
	unsigned n;

	for (n = 0; n < 32; n++) {
		struct msm_endpoint *ept = ui->ept + n;

		ept->ui = ui;
		ept->bit = n;
		ept->num = n & 15;
		ept->ep.name = ep_name[n];
		ept->ep.ops = &msm72k_ep_ops;

		if (ept->bit > 15) {
			/* IN endpoint */
			ept->head = ui->head + (ept->num << 1) + 1;
			ept->flags = EPT_FLAG_IN;
		} else {
			/* OUT endpoint */
			ept->head = ui->head + (ept->num << 1);
			ept->flags = 0;
		}
		setup_timer(&ept->prime_timer, ept_prime_timer_func,
			(unsigned long) ept);

	}
}

static void config_ept(struct msm_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	unsigned cfg = CONFIG_MAX_PKT(ept->ep.maxpacket) | CONFIG_ZLT;
	const struct usb_endpoint_descriptor *desc = ept->ep.desc;
	unsigned mult = 0;

	if (desc && ((desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
				== USB_ENDPOINT_XFER_ISOC)) {
		cfg &= ~(CONFIG_MULT);
		mult = ((ept->ep.maxpacket >> CONFIG_MULT_SHIFT) + 1) & 0x03;
		cfg |= (mult << (ffs(CONFIG_MULT) - 1));
	}

	/* ep0 out needs interrupt-on-setup */
	if (ept->bit == 0)
		cfg |= CONFIG_IOS;

	ept->head->config = cfg;
	ept->head->next = TERMINATE;

	if (ept->ep.maxpacket)
		dev_dbg(&ui->pdev->dev,
			"ept #%d %s max:%d head:%p bit:%d\n",
		       ept->num,
		       (ept->flags & EPT_FLAG_IN) ? "in" : "out",
		       ept->ep.maxpacket, ept->head, ept->bit);
}

static void configure_endpoints(struct usb_info *ui)
{
	unsigned n;

	for (n = 0; n < 32; n++)
		config_ept(ui->ept + n);
}

struct usb_request *usb_ept_alloc_req(struct msm_endpoint *ept,
			unsigned bufsize, gfp_t gfp_flags)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req;

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		goto fail1;

	req->item = dma_pool_alloc(ui->pool, gfp_flags, &req->item_dma);
	if (!req->item)
		goto fail2;

	if (bufsize) {
		req->req.buf = kmalloc(bufsize, gfp_flags);
		if (!req->req.buf)
			goto fail3;
		req->alloced = 1;
	}

	return &req->req;

fail3:
	dma_pool_free(ui->pool, req->item, req->item_dma);
fail2:
	kfree(req);
fail1:
	return 0;
}

static void usb_ept_enable(struct msm_endpoint *ept, int yes,
		unsigned char ep_type)
{
	struct usb_info *ui = ept->ui;
	int in = ept->flags & EPT_FLAG_IN;
	unsigned n;

	n = readl(USB_ENDPTCTRL(ept->num));

	if (in) {
		if (yes) {
			n = (n & (~CTRL_TXT_MASK)) |
				(ep_type << CTRL_TXT_EP_TYPE_SHIFT);
			n |= CTRL_TXE | CTRL_TXR;
		} else
			n &= (~CTRL_TXE);
	} else {
		if (yes) {
			n = (n & (~CTRL_RXT_MASK)) |
				(ep_type << CTRL_RXT_EP_TYPE_SHIFT);
			n |= CTRL_RXE | CTRL_RXR;
		} else
			n &= ~(CTRL_RXE);
	}
	/* complete all the updates to ept->head before enabling endpoint*/
	mb();
	writel(n, USB_ENDPTCTRL(ept->num));

	/* Ensure endpoint is enabled before returning */
	mb();

	dev_dbg(&ui->pdev->dev, "ept %d %s %s\n",
	       ept->num, in ? "in" : "out", yes ? "enabled" : "disabled");
}

static void ept_prime_timer_func(unsigned long data)
{
	struct msm_endpoint *ept = (struct msm_endpoint *)data;
	struct usb_info *ui = ept->ui;
	unsigned n = 1 << ept->bit;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);

	ept->false_prime_fail_count++;
	if ((readl_relaxed(USB_ENDPTPRIME) & n)) {
		/*
		 * ---- UNLIKELY ---
		 * May be hardware is taking long time to process the
		 * prime request. Or could be intermittent priming and
		 * previous dTD is not fired yet.
		 */
		mod_timer(&ept->prime_timer, EPT_PRIME_CHECK_DELAY);
		goto out;
	}
	if (readl_relaxed(USB_ENDPTSTAT) & n)
		goto out;

	/* clear speculative loads on item->info */
	rmb();
	if (ept->req && (ept->req->item->info & INFO_ACTIVE)) {
		ui->prime_fail_count++;
		ept->actual_prime_fail_count++;
		pr_err("%s(): ept%d%s prime failed. ept: config: %x"
				"active: %x next: %x info: %x\n",
				__func__, ept->num,
				ept->flags & EPT_FLAG_IN ? "in" : "out",
				ept->head->config, ept->head->active,
				ept->head->next, ept->head->info);
		writel_relaxed(n, USB_ENDPTPRIME);
		mod_timer(&ept->prime_timer, EPT_PRIME_CHECK_DELAY);
	}
out:
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void usb_ept_start(struct msm_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req = ept->req;
	unsigned n = 1 << ept->bit;

	BUG_ON(req->live);

	while (req) {
		req->live = 1;
		/* prepare the transaction descriptor item for the hardware */
		req->item->info =
			INFO_BYTES(req->req.length) | INFO_IOC | INFO_ACTIVE;
		req->item->page0 = req->dma;
		req->item->page1 = (req->dma + 0x1000) & 0xfffff000;
		req->item->page2 = (req->dma + 0x2000) & 0xfffff000;
		req->item->page3 = (req->dma + 0x3000) & 0xfffff000;

		if (req->next == NULL) {
			req->item->next = TERMINATE;
			break;
		}
		req->item->next = req->next->item_dma;
		req = req->next;
	}

	rmb();
	/* link the hw queue head to the request's transaction item */
	ept->head->next = ept->req->item_dma;
	ept->head->info = 0;

	/* flush buffers before priming ept */
	mb();
	/* during high throughput testing it is observed that
	 * ept stat bit is not set even though all the data
	 * structures are updated properly and ept prime bit
	 * is set. To workaround the issue, kick a timer and
	 * make decision on re-prime. We can do a busy loop here
	 * but it leads to high cpu usage.
	 */
	writel_relaxed(n, USB_ENDPTPRIME);
	mod_timer(&ept->prime_timer, EPT_PRIME_CHECK_DELAY);
}

int usb_ept_queue_xfer(struct msm_endpoint *ept, struct usb_request *_req)
{
	unsigned long flags;
	struct msm_request *req = to_msm_request(_req);
	struct msm_request *last;
	struct usb_info *ui = ept->ui;
	unsigned length = req->req.length;

	if (length > 0x4000)
		return -EMSGSIZE;

	spin_lock_irqsave(&ui->lock, flags);

	if (ept->num != 0 && ept->ep.desc == NULL) {
		req->req.status = -EINVAL;
		spin_unlock_irqrestore(&ui->lock, flags);
		dev_err(&ui->pdev->dev,
			"%s: called for disabled endpoint\n", __func__);
		return -EINVAL;
	}

	if (req->busy) {
		req->req.status = -EBUSY;
		spin_unlock_irqrestore(&ui->lock, flags);
		dev_err(&ui->pdev->dev,
			"usb_ept_queue_xfer() tried to queue busy request\n");
		return -EBUSY;
	}

	if (!atomic_read(&ui->configured) && (ept->num != 0)) {
		req->req.status = -ESHUTDOWN;
		spin_unlock_irqrestore(&ui->lock, flags);
		if (printk_ratelimit())
			dev_err(&ui->pdev->dev,
				"%s: called while offline\n", __func__);
		return -ESHUTDOWN;
	}

	if (ui->usb_state == USB_STATE_SUSPENDED) {
		if (!atomic_read(&ui->remote_wakeup)) {
			req->req.status = -EAGAIN;
			spin_unlock_irqrestore(&ui->lock, flags);
			if (printk_ratelimit())
				dev_err(&ui->pdev->dev,
				"%s: cannot queue as bus is suspended "
				"ept #%d %s max:%d head:%p bit:%d\n",
				__func__, ept->num,
				(ept->flags & EPT_FLAG_IN) ? "in" : "out",
				ept->ep.maxpacket, ept->head, ept->bit);

			return -EAGAIN;
		}

		wake_lock(&ui->wlock);
		usb_phy_set_suspend(ui->xceiv, 0);
		schedule_delayed_work(&ui->rw_work, REMOTE_WAKEUP_DELAY);
	}

	req->busy = 1;
	req->live = 0;
	req->next = 0;
	req->req.status = -EBUSY;

	req->dma = dma_map_single(NULL, req->req.buf, length,
				  (ept->flags & EPT_FLAG_IN) ?
				  DMA_TO_DEVICE : DMA_FROM_DEVICE);


	/* Add the new request to the end of the queue */
	last = ept->last;
	if (last) {
		/* Already requests in the queue. add us to the
		 * end, but let the completion interrupt actually
		 * start things going, to avoid hw issues
		 */
		last->next = req;
		req->prev = last;

	} else {
		/* queue was empty -- kick the hardware */
		ept->req = req;
		req->prev = NULL;
		usb_ept_start(ept);
	}
	ept->last = req;

	spin_unlock_irqrestore(&ui->lock, flags);
	return 0;
}

/* --- endpoint 0 handling --- */

static void ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct msm_request *r = to_msm_request(req);
	struct msm_endpoint *ept = to_msm_endpoint(ep);
	struct usb_info *ui = ept->ui;

	req->complete = r->gadget_complete;
	r->gadget_complete = 0;
	if	(req->complete)
		req->complete(&ui->ep0in.ep, req);
}

static void ep0_status_complete(struct usb_ep *ep, struct usb_request *_req)
{
	struct usb_request *req = _req->context;
	struct msm_request *r;
	struct msm_endpoint *ept;
	struct usb_info *ui;

	pr_debug("%s:\n", __func__);
	if (!req)
		return;

	r = to_msm_request(req);
	ept = to_msm_endpoint(ep);
	ui = ept->ui;
	_req->context = 0;

	req->complete = r->gadget_complete;
	req->zero = 0;
	r->gadget_complete = 0;
	if (req->complete)
		req->complete(&ui->ep0in.ep, req);

}

static void ep0_status_phase(struct usb_ep *ep, struct usb_request *req)
{
	struct msm_endpoint *ept = to_msm_endpoint(ep);
	struct usb_info *ui = ept->ui;

	pr_debug("%s:\n", __func__);

	req->length = 0;
	req->complete = ep0_status_complete;

	/* status phase */
	if (atomic_read(&ui->ep0_dir) == USB_DIR_IN)
		usb_ept_queue_xfer(&ui->ep0out, req);
	else
		usb_ept_queue_xfer(&ui->ep0in, req);
}

static void ep0in_send_zero_leng_pkt(struct msm_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	struct usb_request *req = ui->setup_req;

	pr_debug("%s:\n", __func__);

	req->length = 0;
	req->complete = ep0_status_phase;
	usb_ept_queue_xfer(&ui->ep0in, req);
}

static void ep0_queue_ack_complete(struct usb_ep *ep,
	struct usb_request *_req)
{
	struct msm_endpoint *ept = to_msm_endpoint(ep);
	struct usb_info *ui = ept->ui;
	struct usb_request *req = ui->setup_req;

	pr_debug("%s: _req:%p actual:%d length:%d zero:%d\n",
			__func__, _req, _req->actual,
			_req->length, _req->zero);

	/* queue up the receive of the ACK response from the host */
	if (_req->status == 0 && _req->actual == _req->length) {
		req->context = _req;
		if (atomic_read(&ui->ep0_dir) == USB_DIR_IN) {
			if (_req->zero && _req->length &&
					!(_req->length % ep->maxpacket)) {
				ep0in_send_zero_leng_pkt(&ui->ep0in);
				return;
			}
		}
		ep0_status_phase(ep, req);
	} else
		ep0_complete(ep, _req);
}

static void ep0_setup_ack_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct msm_endpoint *ept = to_msm_endpoint(ep);
	struct usb_info *ui = ept->ui;
	unsigned int temp;
	int test_mode = atomic_read(&ui->test_mode);

	if (!test_mode)
		return;

	switch (test_mode) {
	case J_TEST:
		dev_info(&ui->pdev->dev, "usb electrical test mode: (J)\n");
		temp = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(temp | PORTSC_PTC_J_STATE, USB_PORTSC);
		break;

	case K_TEST:
		dev_info(&ui->pdev->dev, "usb electrical test mode: (K)\n");
		temp = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(temp | PORTSC_PTC_K_STATE, USB_PORTSC);
		break;

	case SE0_NAK_TEST:
		dev_info(&ui->pdev->dev,
			"usb electrical test mode: (SE0-NAK)\n");
		temp = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(temp | PORTSC_PTC_SE0_NAK, USB_PORTSC);
		break;

	case TST_PKT_TEST:
		dev_info(&ui->pdev->dev,
			"usb electrical test mode: (TEST_PKT)\n");
		temp = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(temp | PORTSC_PTC_TST_PKT, USB_PORTSC);
		break;
	}
}

static void ep0_setup_ack(struct usb_info *ui)
{
	struct usb_request *req = ui->setup_req;
	req->length = 0;
	req->complete = ep0_setup_ack_complete;
	usb_ept_queue_xfer(&ui->ep0in, req);
}

static void ep0_setup_stall(struct usb_info *ui)
{
	writel((1<<16) | (1<<0), USB_ENDPTCTRL(0));
}

static void ep0_setup_send(struct usb_info *ui, unsigned length)
{
	struct usb_request *req = ui->setup_req;
	struct msm_request *r = to_msm_request(req);
	struct msm_endpoint *ept = &ui->ep0in;

	req->length = length;
	req->complete = ep0_queue_ack_complete;
	r->gadget_complete = 0;
	usb_ept_queue_xfer(ept, req);
}

static void handle_setup(struct usb_info *ui)
{
	struct usb_ctrlrequest ctl;
	struct usb_request *req = ui->setup_req;
	int ret;
#ifdef CONFIG_USB_OTG
	u8 hnp;
	unsigned long flags;
#endif
	/* USB hardware sometimes generate interrupt before
	 * 8 bytes of SETUP packet are written to system memory.
	 * This results in fetching wrong setup_data sometimes.
	 * TODO: Remove below workaround of adding 1us delay once
	 * it gets fixed in hardware.
	 */
	udelay(10);

	memcpy(&ctl, ui->ep0out.head->setup_data, sizeof(ctl));
	/* Ensure buffer is read before acknowledging to h/w */
	mb();

	writel(EPT_RX(0), USB_ENDPTSETUPSTAT);

	if (ctl.bRequestType & USB_DIR_IN)
		atomic_set(&ui->ep0_dir, USB_DIR_IN);
	else
		atomic_set(&ui->ep0_dir, USB_DIR_OUT);

	/* any pending ep0 transactions must be canceled */
	flush_endpoint(&ui->ep0out);
	flush_endpoint(&ui->ep0in);

	dev_dbg(&ui->pdev->dev,
		"setup: type=%02x req=%02x val=%04x idx=%04x len=%04x\n",
	       ctl.bRequestType, ctl.bRequest, ctl.wValue,
	       ctl.wIndex, ctl.wLength);

	if ((ctl.bRequestType & (USB_DIR_IN | USB_TYPE_MASK)) ==
					(USB_DIR_IN | USB_TYPE_STANDARD)) {
		if (ctl.bRequest == USB_REQ_GET_STATUS) {
			/* OTG supplement Rev 2.0 introduces another device
			 * GET_STATUS request for HNP polling with length = 1.
			 */
			u8 len = 2;
			switch (ctl.bRequestType & USB_RECIP_MASK) {
			case USB_RECIP_ENDPOINT:
			{
				struct msm_endpoint *ept;
				unsigned num =
					ctl.wIndex & USB_ENDPOINT_NUMBER_MASK;
				u16 temp = 0;

				if (num == 0) {
					memset(req->buf, 0, 2);
					break;
				}
				if (ctl.wIndex & USB_ENDPOINT_DIR_MASK)
					num += 16;
				ept = &ui->ep0out + num;
				temp = usb_ep_get_stall(ept);
				temp = temp << USB_ENDPOINT_HALT;
				memcpy(req->buf, &temp, 2);
				break;
			}
			case USB_RECIP_DEVICE:
			{
				u16 temp = 0;

				if (ctl.wIndex == OTG_STATUS_SELECTOR) {
#ifdef CONFIG_USB_OTG
					spin_lock_irqsave(&ui->lock, flags);
					hnp = (ui->gadget.host_request <<
							HOST_REQUEST_FLAG);
					ui->hnp_avail = 1;
					spin_unlock_irqrestore(&ui->lock,
							flags);
					memcpy(req->buf, &hnp, 1);
					len = 1;
#else
					goto stall;
#endif
				} else {
					temp = (atomic_read(&ui->self_powered)
						<< USB_DEVICE_SELF_POWERED);
					temp |= (atomic_read(&ui->remote_wakeup)
						<< USB_DEVICE_REMOTE_WAKEUP);
					memcpy(req->buf, &temp, 2);
				}
				break;
			}
			case USB_RECIP_INTERFACE:
				memset(req->buf, 0, 2);
				break;
			default:
				goto stall;
			}
			ep0_setup_send(ui, len);
			return;
		}
	}
	if (ctl.bRequestType ==
		    (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT)) {
		if ((ctl.bRequest == USB_REQ_CLEAR_FEATURE) ||
				(ctl.bRequest == USB_REQ_SET_FEATURE)) {
			if ((ctl.wValue == 0) && (ctl.wLength == 0)) {
				unsigned num = ctl.wIndex & 0x0f;

				if (num != 0) {
					struct msm_endpoint *ept;

					if (ctl.wIndex & 0x80)
						num += 16;
					ept = &ui->ep0out + num;

					if (ept->wedged)
						goto ack;
					if (ctl.bRequest == USB_REQ_SET_FEATURE)
						usb_ept_set_halt(&ept->ep, 1);
					else
						usb_ept_set_halt(&ept->ep, 0);
				}
				goto ack;
			}
		}
	}
	if (ctl.bRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD)) {
		if (ctl.bRequest == USB_REQ_SET_CONFIGURATION) {
			atomic_set(&ui->configured, !!ctl.wValue);
			msm_hsusb_set_state(USB_STATE_CONFIGURED);
		} else if (ctl.bRequest == USB_REQ_SET_ADDRESS) {
			/*
			 * Gadget speed should be set when PCI interrupt
			 * occurs. But sometimes, PCI interrupt is not
			 * occuring after reset. Hence update the gadget
			 * speed here.
			 */
			if (ui->gadget.speed == USB_SPEED_UNKNOWN) {
				dev_info(&ui->pdev->dev,
					"PCI intr missed"
					"set speed explictly\n");
				msm_hsusb_set_speed(ui);
			}
			msm_hsusb_set_state(USB_STATE_ADDRESS);

			/* write address delayed (will take effect
			** after the next IN txn)
			*/
			writel((ctl.wValue << 25) | (1 << 24), USB_DEVICEADDR);
			goto ack;
		} else if (ctl.bRequest == USB_REQ_SET_FEATURE) {
			switch (ctl.wValue) {
			case USB_DEVICE_TEST_MODE:
				switch (ctl.wIndex) {
				case J_TEST:
				case K_TEST:
				case SE0_NAK_TEST:
				case TST_PKT_TEST:
					atomic_set(&ui->test_mode, ctl.wIndex);
					goto ack;
				}
				goto stall;
			case USB_DEVICE_REMOTE_WAKEUP:
				atomic_set(&ui->remote_wakeup, 1);
				goto ack;
#ifdef CONFIG_USB_OTG
			case USB_DEVICE_B_HNP_ENABLE:
				ui->gadget.b_hnp_enable = 1;
				goto ack;
			case USB_DEVICE_A_HNP_SUPPORT:
			case USB_DEVICE_A_ALT_HNP_SUPPORT:
				/* B-devices compliant to OTG spec
				 * Rev 2.0 are not required to
				 * suppport these features.
				 */
				goto stall;
#endif
			}
		} else if ((ctl.bRequest == USB_REQ_CLEAR_FEATURE) &&
				(ctl.wValue == USB_DEVICE_REMOTE_WAKEUP)) {
			atomic_set(&ui->remote_wakeup, 0);
			goto ack;
		}
	}

	/* delegate if we get here */
	if (ui->driver) {
		ret = ui->driver->setup(&ui->gadget, &ctl);
		if (ret >= 0)
			return;
	}

stall:
	/* stall ep0 on error */
	ep0_setup_stall(ui);
	return;

ack:
	ep0_setup_ack(ui);
}

static void handle_endpoint(struct usb_info *ui, unsigned bit)
{
	struct msm_endpoint *ept = ui->ept + bit;
	struct msm_request *req;
	unsigned long flags;
	int req_dequeue = 1;
	int dtd_update_fail_count_chk = 10;
	int check_bit = 0;
	unsigned info;

	/*
	INFO("handle_endpoint() %d %s req=%p(%08x)\n",
		ept->num, (ept->flags & EPT_FLAG_IN) ? "in" : "out",
		ept->req, ept->req ? ept->req->item_dma : 0);
	*/

	/* expire all requests that are no longer active */
	spin_lock_irqsave(&ui->lock, flags);
	while ((req = ept->req)) {
		/* if we've processed all live requests, time to
		 * restart the hardware on the next non-live request
		 */
		if (!req->live) {
			usb_ept_start(ept);
			break;
		}

dequeue:
		/* clean speculative fetches on req->item->info */
		dma_coherent_post_ops();
		info = req->item->info;
		/* if the transaction is still in-flight, stop here */
		if (info & INFO_ACTIVE) {
			if (req_dequeue) {
				ui->dTD_update_fail_count++;
				ept->dTD_update_fail_count++;
				udelay(1);
				if (!dtd_update_fail_count_chk--) {
					req_dequeue = 0;
					check_bit = 1;
				}
				goto dequeue;
			} else {
				if (check_bit) {
					pr_debug("%s: Delay Workaround Failed\n",
						 __func__);
					check_bit = 0;
					ui->dTD_workaround_fail_count++;
					ept->dTD_workaround_fail_count++;
				}
				break;
			}
		}
		req_dequeue = 0;

		del_timer(&ept->prime_timer);
		/* advance ept queue to the next request */
		ept->req = req->next;
		if (ept->req == 0)
			ept->last = 0;

		dma_unmap_single(NULL, req->dma, req->req.length,
				 (ept->flags & EPT_FLAG_IN) ?
				 DMA_TO_DEVICE : DMA_FROM_DEVICE);

		if (info & (INFO_HALTED | INFO_BUFFER_ERROR | INFO_TXN_ERROR)) {
			/* XXX pass on more specific error code */
			req->req.status = -EIO;
			req->req.actual = 0;
			dev_err(&ui->pdev->dev,
				"ept %d %s error. info=%08x\n",
			       ept->num,
			       (ept->flags & EPT_FLAG_IN) ? "in" : "out",
			       info);
		} else {
			req->req.status = 0;
			req->req.actual =
				req->req.length - ((info >> 16) & 0x7FFF);
		}
		req->busy = 0;
		req->live = 0;

		if (req->req.complete) {
			spin_unlock_irqrestore(&ui->lock, flags);
			req->req.complete(&ept->ep, &req->req);
			spin_lock_irqsave(&ui->lock, flags);
		}
	}
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void flush_endpoint_hw(struct usb_info *ui, unsigned bits)
{
	/* flush endpoint, canceling transactions
	** - this can take a "large amount of time" (per databook)
	** - the flush can fail in some cases, thus we check STAT
	**   and repeat if we're still operating
	**   (does the fact that this doesn't use the tripwire matter?!)
	*/
	do {
		writel(bits, USB_ENDPTFLUSH);
		while (readl(USB_ENDPTFLUSH) & bits)
			udelay(100);
	} while (readl(USB_ENDPTSTAT) & bits);
}

static void flush_endpoint_sw(struct msm_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req, *next_req = NULL;
	unsigned long flags;

	/* inactive endpoints have nothing to do here */
	if (ept->ep.maxpacket == 0)
		return;

	/* put the queue head in a sane state */
	ept->head->info = 0;
	ept->head->next = TERMINATE;

	/* cancel any pending requests */
	spin_lock_irqsave(&ui->lock, flags);
	req = ept->req;
	ept->req = 0;
	ept->last = 0;
	while (req != 0) {
		req->busy = 0;
		req->live = 0;
		req->req.status = -ESHUTDOWN;
		req->req.actual = 0;

		/* Gadget driver may free the request in completion
		 * handler. So keep a copy of next req pointer
		 * before calling completion handler.
		 */
		next_req = req->next;
		if (req->req.complete) {
			spin_unlock_irqrestore(&ui->lock, flags);
			req->req.complete(&ept->ep, &req->req);
			spin_lock_irqsave(&ui->lock, flags);
		}
		req = next_req;
	}
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void flush_endpoint(struct msm_endpoint *ept)
{
	del_timer(&ept->prime_timer);
	flush_endpoint_hw(ept->ui, (1 << ept->bit));
	flush_endpoint_sw(ept);
}

static irqreturn_t usb_interrupt(int irq, void *data)
{
	struct usb_info *ui = data;
	struct msm_otg *dev = to_msm_otg(ui->xceiv);
	unsigned n;
	unsigned long flags;

	if (atomic_read(&dev->in_lpm))
		return IRQ_NONE;

	n = readl(USB_USBSTS);
	writel(n, USB_USBSTS);

	/* somehow we got an IRQ while in the reset sequence: ignore it */
	if (!atomic_read(&ui->running))
		return IRQ_HANDLED;

	if (n & STS_PCI) {
		msm_hsusb_set_speed(ui);
		if (atomic_read(&ui->configured)) {
			wake_lock(&ui->wlock);

			spin_lock_irqsave(&ui->lock, flags);
			ui->usb_state = USB_STATE_CONFIGURED;
			ui->flags = USB_FLAG_CONFIGURED;
			spin_unlock_irqrestore(&ui->lock, flags);

			ui->driver->resume(&ui->gadget);
			schedule_work(&ui->work);
		} else {
			msm_hsusb_set_state(USB_STATE_DEFAULT);
		}

#ifdef CONFIG_USB_OTG
		/* notify otg to clear A_BIDL_ADIS timer */
		if (ui->gadget.is_a_peripheral)
			usb_phy_set_suspend(ui->xceiv, 0);
#endif
	}

	if (n & STS_URI) {
		dev_dbg(&ui->pdev->dev, "reset\n");
		spin_lock_irqsave(&ui->lock, flags);
		ui->gadget.speed = USB_SPEED_UNKNOWN;
		spin_unlock_irqrestore(&ui->lock, flags);
#ifdef CONFIG_USB_OTG
		/* notify otg to clear A_BIDL_ADIS timer */
		if (ui->gadget.is_a_peripheral)
			usb_phy_set_suspend(ui->xceiv, 0);
		spin_lock_irqsave(&ui->lock, flags);
		/* Host request is persistent across reset */
		ui->gadget.b_hnp_enable = 0;
		ui->hnp_avail = 0;
		spin_unlock_irqrestore(&ui->lock, flags);
#endif
		msm_hsusb_set_state(USB_STATE_DEFAULT);
		atomic_set(&ui->remote_wakeup, 0);
		if (!ui->gadget.is_a_peripheral)
			schedule_delayed_work(&ui->chg_stop, 0);

		writel(readl(USB_ENDPTSETUPSTAT), USB_ENDPTSETUPSTAT);
		writel(readl(USB_ENDPTCOMPLETE), USB_ENDPTCOMPLETE);
		writel(0xffffffff, USB_ENDPTFLUSH);
		writel(0, USB_ENDPTCTRL(1));

		wake_lock(&ui->wlock);
		if (atomic_read(&ui->configured)) {
			/* marking us offline will cause ept queue attempts
			** to fail
			*/
			atomic_set(&ui->configured, 0);
			/* Defer sending offline uevent to userspace */
			atomic_set(&ui->offline_pending, 1);

			/* XXX: we can't seem to detect going offline,
			 * XXX:  so deconfigure on reset for the time being
			 */
			dev_dbg(&ui->pdev->dev,
					"usb: notify offline\n");
			ui->driver->disconnect(&ui->gadget);
			/* cancel pending ep0 transactions */
			flush_endpoint(&ui->ep0out);
			flush_endpoint(&ui->ep0in);

		}
		/* Start phy stuck timer */
		if (ui->pdata && ui->pdata->is_phy_status_timer_on)
			mod_timer(&phy_status_timer, PHY_STATUS_CHECK_DELAY);
	}

	if (n & STS_SLI) {
		dev_dbg(&ui->pdev->dev, "suspend\n");

		spin_lock_irqsave(&ui->lock, flags);
		ui->usb_state = USB_STATE_SUSPENDED;
		ui->flags = USB_FLAG_SUSPEND;
		spin_unlock_irqrestore(&ui->lock, flags);

		ui->driver->suspend(&ui->gadget);
		schedule_work(&ui->work);
#ifdef CONFIG_USB_OTG
		/* notify otg for
		 * 1. kicking A_BIDL_ADIS timer in case of A-peripheral
		 * 2. disabling pull-up and kicking B_ASE0_RST timer
		 */
		if (ui->gadget.b_hnp_enable || ui->gadget.is_a_peripheral)
			usb_phy_set_suspend(ui->xceiv, 1);
#endif
	}

	if (n & STS_UI) {
		n = readl(USB_ENDPTSETUPSTAT);
		if (n & EPT_RX(0))
			handle_setup(ui);

		n = readl(USB_ENDPTCOMPLETE);
		writel(n, USB_ENDPTCOMPLETE);
		while (n) {
			unsigned bit = __ffs(n);
			handle_endpoint(ui, bit);
			n = n & (~(1 << bit));
		}
	}
	return IRQ_HANDLED;
}

static void usb_prepare(struct usb_info *ui)
{
	spin_lock_init(&ui->lock);

	memset(ui->buf, 0, 4096);
	ui->head = (void *) (ui->buf + 0);

	/* only important for reset/reinit */
	memset(ui->ept, 0, sizeof(ui->ept));
	ui->next_item = 0;
	ui->next_ifc_num = 0;

	init_endpoints(ui);

	ui->ep0in.ep.maxpacket = 64;
	ui->ep0out.ep.maxpacket = 64;

	ui->setup_req =
		usb_ept_alloc_req(&ui->ep0in, SETUP_BUF_SIZE, GFP_KERNEL);

	INIT_WORK(&ui->work, usb_do_work);
	INIT_DELAYED_WORK(&ui->chg_det, usb_chg_detect);
	INIT_DELAYED_WORK(&ui->chg_stop, usb_chg_stop);
	INIT_DELAYED_WORK(&ui->rw_work, usb_do_remote_wakeup);
	if (ui->pdata && ui->pdata->is_phy_status_timer_on)
		INIT_WORK(&ui->phy_status_check, usb_phy_stuck_recover);
}

static void usb_reset(struct usb_info *ui)
{
	struct msm_otg *otg = to_msm_otg(ui->xceiv);

	dev_dbg(&ui->pdev->dev, "reset controller\n");

	atomic_set(&ui->running, 0);

	/*
	 * PHY reset takes minimum 100 msec. Hence reset only link
	 * during HNP. Reset PHY and link in B-peripheral mode.
	 */
	if (ui->gadget.is_a_peripheral)
		otg->reset(ui->xceiv, 0);
	else
		otg->reset(ui->xceiv, 1);

	/* set usb controller interrupt threshold to zero*/
	writel((readl(USB_USBCMD) & ~USBCMD_ITC_MASK) | USBCMD_ITC(0),
							USB_USBCMD);

	writel(ui->dma, USB_ENDPOINTLISTADDR);

	configure_endpoints(ui);

	/* marking us offline will cause ept queue attempts to fail */
	atomic_set(&ui->configured, 0);

	if (ui->driver) {
		dev_dbg(&ui->pdev->dev, "usb: notify offline\n");
		ui->driver->disconnect(&ui->gadget);
	}

	/* cancel pending ep0 transactions */
	flush_endpoint(&ui->ep0out);
	flush_endpoint(&ui->ep0in);

	/* enable interrupts */
	writel(STS_URI | STS_SLI | STS_UI | STS_PCI, USB_USBINTR);

	/* Ensure that h/w RESET is completed before returning */
	mb();

	atomic_set(&ui->running, 1);
}

static void usb_start(struct usb_info *ui)
{
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	ui->flags |= USB_FLAG_START;
	schedule_work(&ui->work);
	spin_unlock_irqrestore(&ui->lock, flags);
}

static int usb_free(struct usb_info *ui, int ret)
{
	if (ret)
		dev_dbg(&ui->pdev->dev, "usb_free(%d)\n", ret);

	usb_del_gadget_udc(&ui->gadget);

	if (ui->xceiv)
		usb_put_transceiver(ui->xceiv);

	if (ui->irq)
		free_irq(ui->irq, 0);
	if (ui->pool)
		dma_pool_destroy(ui->pool);
	if (ui->dma)
		dma_free_coherent(&ui->pdev->dev, 4096, ui->buf, ui->dma);
	kfree(ui);
	return ret;
}

static void usb_do_work_check_vbus(struct usb_info *ui)
{
	unsigned long iflags;

	spin_lock_irqsave(&ui->lock, iflags);
	if (is_usb_online(ui))
		ui->flags |= USB_FLAG_VBUS_ONLINE;
	else
		ui->flags |= USB_FLAG_VBUS_OFFLINE;
	spin_unlock_irqrestore(&ui->lock, iflags);
}

static void usb_do_work(struct work_struct *w)
{
	struct usb_info *ui = container_of(w, struct usb_info, work);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	unsigned long iflags;
	unsigned flags, _vbus;

	for (;;) {
		spin_lock_irqsave(&ui->lock, iflags);
		flags = ui->flags;
		ui->flags = 0;
		_vbus = is_usb_online(ui);
		spin_unlock_irqrestore(&ui->lock, iflags);

		/* give up if we have nothing to do */
		if (flags == 0)
			break;

		switch (ui->state) {
		case USB_STATE_IDLE:
			if (flags & USB_FLAG_START) {
				int ret;

				if (!_vbus) {
					ui->state = USB_STATE_OFFLINE;
					break;
				}

				pm_runtime_get_noresume(&ui->pdev->dev);
				pm_runtime_resume(&ui->pdev->dev);
				dev_dbg(&ui->pdev->dev,
					"msm72k_udc: IDLE -> ONLINE\n");
				usb_reset(ui);
				ret = request_irq(otg->irq, usb_interrupt,
							IRQF_SHARED,
							ui->pdev->name, ui);
				/* FIXME: should we call BUG_ON when
				 * requst irq fails
				 */
				if (ret) {
					dev_err(&ui->pdev->dev,
						"hsusb: peripheral: request irq"
						" failed:(%d)", ret);
					break;
				}
				ui->irq = otg->irq;
				ui->state = USB_STATE_ONLINE;
				usb_do_work_check_vbus(ui);

				if (!atomic_read(&ui->softconnect))
					break;

				msm72k_pullup_internal(&ui->gadget, 1);

				if (!ui->gadget.is_a_peripheral)
					schedule_delayed_work(
							&ui->chg_det,
							USB_CHG_DET_DELAY);

			}
			break;
		case USB_STATE_ONLINE:
			if (atomic_read(&ui->offline_pending)) {
				switch_set_state(&ui->sdev, 0);
				atomic_set(&ui->offline_pending, 0);
			}

			/* If at any point when we were online, we received
			 * the signal to go offline, we must honor it
			 */
			if (flags & USB_FLAG_VBUS_OFFLINE) {

				ui->chg_current = 0;
				/* wait incase chg_detect is running */
				if (!ui->gadget.is_a_peripheral)
					cancel_delayed_work_sync(&ui->chg_det);

				dev_dbg(&ui->pdev->dev,
					"msm72k_udc: ONLINE -> OFFLINE\n");

				atomic_set(&ui->running, 0);
				atomic_set(&ui->remote_wakeup, 0);
				atomic_set(&ui->configured, 0);

				if (ui->driver) {
					dev_dbg(&ui->pdev->dev,
						"usb: notify offline\n");
					ui->driver->disconnect(&ui->gadget);
				}
				/* cancel pending ep0 transactions */
				flush_endpoint(&ui->ep0out);
				flush_endpoint(&ui->ep0in);

				/* synchronize with irq context */
				spin_lock_irqsave(&ui->lock, iflags);
#ifdef CONFIG_USB_OTG
				ui->gadget.host_request = 0;
				ui->gadget.b_hnp_enable = 0;
				ui->hnp_avail = 0;
#endif
				msm72k_pullup_internal(&ui->gadget, 0);
				spin_unlock_irqrestore(&ui->lock, iflags);


				/* if charger is initialized to known type
				 * we must let modem know about charger
				 * disconnection
				 */
				usb_phy_set_power(ui->xceiv, 0);

				if (ui->irq) {
					/* Disable and acknowledge all
					 * USB interrupts before freeing
					 * irq, so that no USB spurious
					 * interrupt occurs during USB cable
					 * disconnect which may lead to
					 * IRQ nobody cared error.
					 */
					writel_relaxed(0, USB_USBINTR);
					writel_relaxed(readl_relaxed(USB_USBSTS)
								, USB_USBSTS);
					/* Ensure that above STOREs are
					 * completed before enabling
					 * interrupts */
					wmb();
					free_irq(ui->irq, ui);
					ui->irq = 0;
				}


				switch_set_state(&ui->sdev, 0);

				ui->state = USB_STATE_OFFLINE;
				usb_do_work_check_vbus(ui);
				pm_runtime_put_noidle(&ui->pdev->dev);
				pm_runtime_suspend(&ui->pdev->dev);
				wake_unlock(&ui->wlock);
				break;
			}
			if (flags & USB_FLAG_SUSPEND) {
				int maxpower = usb_get_max_power(ui);

				if (maxpower < 0)
					break;

				usb_phy_set_power(ui->xceiv, 0);
				/* To support TCXO during bus suspend
				 * This might be dummy check since bus suspend
				 * is not implemented as of now
				 * */
				if (release_wlocks)
					wake_unlock(&ui->wlock);

				/* TBD: Initiate LPM at usb bus suspend */
				break;
			}
			if (flags & USB_FLAG_CONFIGURED) {
				int maxpower = usb_get_max_power(ui);

				/* We may come here even when no configuration
				 * is selected. Send online/offline event
				 * accordingly.
				 */
				switch_set_state(&ui->sdev,
						atomic_read(&ui->configured));

				if (maxpower < 0)
					break;

				ui->chg_current = maxpower;
				usb_phy_set_power(ui->xceiv, maxpower);
				break;
			}
			if (flags & USB_FLAG_RESET) {
				dev_dbg(&ui->pdev->dev,
					"msm72k_udc: ONLINE -> RESET\n");
				msm72k_pullup_internal(&ui->gadget, 0);
				usb_reset(ui);
				msm72k_pullup_internal(&ui->gadget, 1);
				dev_dbg(&ui->pdev->dev,
					"msm72k_udc: RESET -> ONLINE\n");
				break;
			}
			break;
		case USB_STATE_OFFLINE:
			/* If we were signaled to go online and vbus is still
			 * present when we received the signal, go online.
			 */
			if ((flags & USB_FLAG_VBUS_ONLINE) && _vbus) {
				int ret;

				pm_runtime_get_noresume(&ui->pdev->dev);
				pm_runtime_resume(&ui->pdev->dev);
				dev_dbg(&ui->pdev->dev,
					"msm72k_udc: OFFLINE -> ONLINE\n");

				usb_reset(ui);
				ui->state = USB_STATE_ONLINE;
				usb_do_work_check_vbus(ui);
				ret = request_irq(otg->irq, usb_interrupt,
							IRQF_SHARED,
							ui->pdev->name, ui);
				/* FIXME: should we call BUG_ON when
				 * requst irq fails
				 */
				if (ret) {
					dev_err(&ui->pdev->dev,
						"hsusb: peripheral: request irq"
						" failed:(%d)", ret);
					break;
				}
				ui->irq = otg->irq;
				enable_irq_wake(otg->irq);

				if (!atomic_read(&ui->softconnect))
					break;
				msm72k_pullup_internal(&ui->gadget, 1);

				if (!ui->gadget.is_a_peripheral)
					schedule_delayed_work(
							&ui->chg_det,
							USB_CHG_DET_DELAY);
			}
			break;
		}
	}
}

/* FIXME - the callers of this function should use a gadget API instead.
 * This is called from htc_battery.c and board-halibut.c
 * WARNING - this can get called before this driver is initialized.
 */
void msm_hsusb_set_vbus_state(int online)
{
	unsigned long flags;
	struct usb_info *ui = the_usb_info;

	if (!ui) {
		pr_err("%s called before driver initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&ui->lock, flags);

	if (is_usb_online(ui) ==  online)
		goto out;

	if (online) {
		ui->usb_state = USB_STATE_POWERED;
		ui->flags |= USB_FLAG_VBUS_ONLINE;
	} else {
		ui->gadget.speed = USB_SPEED_UNKNOWN;
		ui->usb_state = USB_STATE_NOTATTACHED;
		ui->flags |= USB_FLAG_VBUS_OFFLINE;
		ui->chg_type_retry_cnt = 0;
		ui->proprietary_chg = false;
	}
	if (in_interrupt()) {
		schedule_work(&ui->work);
	} else {
		spin_unlock_irqrestore(&ui->lock, flags);
		usb_do_work(&ui->work);
		return;
	}
out:
	spin_unlock_irqrestore(&ui->lock, flags);
}

#if defined(CONFIG_DEBUG_FS)

void usb_function_reenumerate(void)
{
	struct usb_info *ui = the_usb_info;

	/* disable and re-enable the D+ pullup */
	dev_dbg(&ui->pdev->dev, "disable pullup\n");
	writel(readl(USB_USBCMD) & ~USBCMD_RS, USB_USBCMD);

	msleep(10);

	dev_dbg(&ui->pdev->dev, "enable pullup\n");
	writel(readl(USB_USBCMD) | USBCMD_RS, USB_USBCMD);
}

static char debug_buffer[PAGE_SIZE];

static ssize_t debug_read_status(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	char *buf = debug_buffer;
	unsigned long flags;
	struct msm_endpoint *ept;
	struct msm_request *req;
	int n;
	int i = 0;

	spin_lock_irqsave(&ui->lock, flags);

	i += scnprintf(buf + i, PAGE_SIZE - i,
		   "regs: setup=%08x prime=%08x stat=%08x done=%08x\n",
		   readl(USB_ENDPTSETUPSTAT),
		   readl(USB_ENDPTPRIME),
		   readl(USB_ENDPTSTAT),
		   readl(USB_ENDPTCOMPLETE));
	i += scnprintf(buf + i, PAGE_SIZE - i,
		   "regs:   cmd=%08x   sts=%08x intr=%08x port=%08x\n\n",
		   readl(USB_USBCMD),
		   readl(USB_USBSTS),
		   readl(USB_USBINTR),
		   readl(USB_PORTSC));


	for (n = 0; n < 32; n++) {
		ept = ui->ept + n;
		if (ept->ep.maxpacket == 0)
			continue;

		i += scnprintf(buf + i, PAGE_SIZE - i,
			"ept%d %s cfg=%08x active=%08x next=%08x info=%08x\n",
			ept->num, (ept->flags & EPT_FLAG_IN) ? "in " : "out",
			ept->head->config, ept->head->active,
			ept->head->next, ept->head->info);

		for (req = ept->req; req; req = req->next)
			i += scnprintf(buf + i, PAGE_SIZE - i,
			"  req @%08x next=%08x info=%08x page0=%08x %c %c\n",
				req->item_dma, req->item->next,
				req->item->info, req->item->page0,
				req->busy ? 'B' : ' ',
				req->live ? 'L' : ' ');
	}

	i += scnprintf(buf + i, PAGE_SIZE - i,
			   "phy failure count: %d\n", ui->phy_fail_count);

	spin_unlock_irqrestore(&ui->lock, flags);

	return simple_read_from_buffer(ubuf, count, ppos, buf, i);
}

static ssize_t debug_write_reset(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	ui->flags |= USB_FLAG_RESET;
	schedule_work(&ui->work);
	spin_unlock_irqrestore(&ui->lock, flags);

	return count;
}

static ssize_t debug_write_cycle(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	usb_function_reenumerate();
	return count;
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

const struct file_operations debug_stat_ops = {
	.open = debug_open,
	.read = debug_read_status,
};

const struct file_operations debug_reset_ops = {
	.open = debug_open,
	.write = debug_write_reset,
};

const struct file_operations debug_cycle_ops = {
	.open = debug_open,
	.write = debug_write_cycle,
};

static ssize_t debug_read_release_wlocks(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	char kbuf[10];
	size_t c = 0;

	memset(kbuf, 0, 10);

	c = scnprintf(kbuf, 10, "%d", release_wlocks);

	if (copy_to_user(ubuf, kbuf, c))
		return -EFAULT;

	return c;
}
static ssize_t debug_write_release_wlocks(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	char kbuf[10];
	long temp;

	memset(kbuf, 0, 10);

	if (copy_from_user(kbuf, buf, count > 10 ? 10 : count))
		return -EFAULT;

	if (strict_strtol(kbuf, 10, &temp))
		return -EINVAL;

	if (temp)
		release_wlocks = 1;
	else
		release_wlocks = 0;

	return count;
}
static int debug_wake_lock_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}
const struct file_operations debug_wlocks_ops = {
	.open = debug_wake_lock_open,
	.read = debug_read_release_wlocks,
	.write = debug_write_release_wlocks,
};

static ssize_t debug_reprime_ep(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	struct msm_endpoint *ept;
	char kbuf[10];
	unsigned int ep_num, dir;
	unsigned long flags;
	unsigned n, i;

	memset(kbuf, 0, 10);

	if (copy_from_user(kbuf, ubuf, count > 10 ? 10 : count))
		return -EFAULT;

	if (sscanf(kbuf, "%u %u", &ep_num, &dir) != 2)
		return -EINVAL;

	if (dir)
		i = ep_num + 16;
	else
		i = ep_num;

	spin_lock_irqsave(&ui->lock, flags);
	ept = ui->ept + i;
	n = 1 << ept->bit;

	if ((readl_relaxed(USB_ENDPTPRIME) & n))
		goto out;

	if (readl_relaxed(USB_ENDPTSTAT) & n)
		goto out;

	/* clear speculative loads on item->info */
	rmb();
	if (ept->req && (ept->req->item->info & INFO_ACTIVE)) {
		pr_err("%s(): ept%d%s prime failed. ept: config: %x"
				"active: %x next: %x info: %x\n",
				__func__, ept->num,
				ept->flags & EPT_FLAG_IN ? "in" : "out",
				ept->head->config, ept->head->active,
				ept->head->next, ept->head->info);
		writel_relaxed(n, USB_ENDPTPRIME);
	}
out:
	spin_unlock_irqrestore(&ui->lock, flags);

	return count;
}

static char buffer[512];
static ssize_t debug_prime_fail_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	char *buf = buffer;
	unsigned long flags;
	struct msm_endpoint *ept;
	int n;
	int i = 0;

	spin_lock_irqsave(&ui->lock, flags);
	for (n = 0; n < 32; n++) {
		ept = ui->ept + n;
		if (ept->ep.maxpacket == 0)
			continue;

		i += scnprintf(buf + i, PAGE_SIZE - i,
			"ept%d %s false_prime_count=%lu prime_fail_count=%d "
					 "dtd_fail_count=%lu "
					 "dTD_workaround_fail_count=%lu\n",
			ept->num, (ept->flags & EPT_FLAG_IN) ? "in " : "out",
			ept->false_prime_fail_count,
			ept->actual_prime_fail_count,
			ept->dTD_update_fail_count,
			ept->dTD_workaround_fail_count);
	}

	i += scnprintf(buf + i, PAGE_SIZE - i,
			   "dTD_update_fail count: %lu\n",
			    ui->dTD_update_fail_count);

	i += scnprintf(buf + i, PAGE_SIZE - i,
			   "prime_fail count: %d\n", ui->prime_fail_count);

	i += scnprintf(buf + i, PAGE_SIZE - i,
			   "dtd_workaround_fail count: %lu\n",
			   ui->dTD_workaround_fail_count);

	spin_unlock_irqrestore(&ui->lock, flags);

	return simple_read_from_buffer(ubuf, count, ppos, buf, i);
}

static int debug_prime_fail_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

const struct file_operations prime_fail_ops = {
	.open = debug_prime_fail_open,
	.read = debug_prime_fail_read,
	.write = debug_reprime_ep,
};

static ssize_t debug_prop_chg_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	char kbuf[2];

	memset(kbuf, 0, sizeof(kbuf));

	if (copy_from_user(kbuf, buf, sizeof(kbuf)))
		return -EFAULT;

	if (!strncmp(kbuf, "1", 1))
		ui->pdata->prop_chg = 1;
	else
		ui->pdata->prop_chg = 0;

	return count;
}

static ssize_t debug_prop_chg_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	char kbuf[2];
	size_t c = 0;

	memset(kbuf, 0, sizeof(kbuf));

	c = scnprintf(kbuf, sizeof(kbuf), "%d\n", ui->pdata->prop_chg);

	if (copy_to_user(ubuf, kbuf, c))
		return -EFAULT;

	return simple_read_from_buffer(ubuf, count, ppos, kbuf, c);
}

static int debug_prop_chg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

const struct file_operations debug_prop_chg_ops = {
	.open = debug_prop_chg_open,
	.read = debug_prop_chg_read,
	.write = debug_prop_chg_write,
};

static void usb_debugfs_init(struct usb_info *ui)
{
	struct dentry *dent;
	dent = debugfs_create_dir(dev_name(&ui->pdev->dev), 0);
	if (IS_ERR(dent))
		return;

	debugfs_create_file("status", 0444, dent, ui, &debug_stat_ops);
	debugfs_create_file("reset", 0222, dent, ui, &debug_reset_ops);
	debugfs_create_file("cycle", 0222, dent, ui, &debug_cycle_ops);
	debugfs_create_file("release_wlocks", 0666, dent, ui,
						&debug_wlocks_ops);
	debugfs_create_file("prime_fail_countt", 0666, dent, ui,
						&prime_fail_ops);
	debugfs_create_file("proprietary_chg", 0666, dent, ui,
						&debug_prop_chg_ops);
}
#else
static void usb_debugfs_init(struct usb_info *ui) {}
#endif

static int
msm72k_enable(struct usb_ep *_ep, const struct usb_endpoint_descriptor *desc)
{
	struct msm_endpoint *ept;
	unsigned char ep_type;

	if (_ep == NULL || desc == NULL)
		return -EINVAL;

	ept = to_msm_endpoint(_ep);
	ep_type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	_ep->maxpacket = le16_to_cpu(desc->wMaxPacketSize);
	config_ept(ept);
	ept->wedged = 0;
	usb_ept_enable(ept, 1, ep_type);
	return 0;
}

static int msm72k_disable(struct usb_ep *_ep)
{
	struct msm_endpoint *ept = to_msm_endpoint(_ep);

	usb_ept_enable(ept, 0, 0);
	flush_endpoint(ept);
	/*
	 * Clear descriptors here. Otherwise previous descriptors
	 * will be used by function drivers upon next enumeration.
	 */
	_ep->desc = NULL;
	return 0;
}

static struct usb_request *
msm72k_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	return usb_ept_alloc_req(to_msm_endpoint(_ep), 0, gfp_flags);
}

static void
msm72k_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct msm_request *req = to_msm_request(_req);
	struct msm_endpoint *ept = to_msm_endpoint(_ep);
	struct usb_info *ui = ept->ui;

	/* request should not be busy */
	BUG_ON(req->busy);
	if (req->alloced)
		kfree(req->req.buf);
	dma_pool_free(ui->pool, req->item, req->item_dma);
	kfree(req);
}

static int
msm72k_queue(struct usb_ep *_ep, struct usb_request *req, gfp_t gfp_flags)
{
	struct msm_endpoint *ep = to_msm_endpoint(_ep);
	struct usb_info *ui = ep->ui;

	if (ep == &ui->ep0in) {
		struct msm_request *r = to_msm_request(req);
		if (!req->length)
			goto ep_queue_done;
		r->gadget_complete = req->complete;
		/* ep0_queue_ack_complete queue a receive for ACK before
		** calling req->complete
		*/
		req->complete = ep0_queue_ack_complete;
		if (atomic_read(&ui->ep0_dir) == USB_DIR_OUT)
			ep = &ui->ep0out;
		goto ep_queue_done;
	}

ep_queue_done:
	return usb_ept_queue_xfer(ep, req);
}

static int msm72k_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct msm_endpoint *ep = to_msm_endpoint(_ep);
	struct msm_request *req = to_msm_request(_req);
	struct usb_info *ui = ep->ui;

	struct msm_request *temp_req;
	unsigned long flags;

	if (!(ui && req && ep->req))
		return -EINVAL;

	spin_lock_irqsave(&ui->lock, flags);
	if (!req->busy) {
		dev_dbg(&ui->pdev->dev, "%s: !req->busy\n", __func__);
		spin_unlock_irqrestore(&ui->lock, flags);
		return -EINVAL;
	}
	del_timer(&ep->prime_timer);
	/* Stop the transfer */
	do {
		writel((1 << ep->bit), USB_ENDPTFLUSH);
		while (readl(USB_ENDPTFLUSH) & (1 << ep->bit))
			udelay(100);
	} while (readl(USB_ENDPTSTAT) & (1 << ep->bit));

	req->req.status = 0;
	req->busy = 0;

	if (ep->req == req) {
		ep->req = req->next;
		ep->head->next = req->item->next;
	} else {
		req->prev->next = req->next;
		if (req->next)
			req->next->prev = req->prev;
		req->prev->item->next = req->item->next;
	}

	if (!req->next)
		ep->last = req->prev;

	/* initialize request to default */
	req->item->next = TERMINATE;
	req->item->info = 0;
	req->live = 0;
	dma_unmap_single(NULL, req->dma, req->req.length,
		(ep->flags & EPT_FLAG_IN) ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE);

	if (req->req.complete) {
		req->req.status = -ECONNRESET;
		spin_unlock_irqrestore(&ui->lock, flags);
		req->req.complete(&ep->ep, &req->req);
		spin_lock_irqsave(&ui->lock, flags);
	}

	if (!req->live) {
		/* Reprime the endpoint for the remaining transfers */
		for (temp_req = ep->req ; temp_req ; temp_req = temp_req->next)
			temp_req->live = 0;
		if (ep->req)
			usb_ept_start(ep);
		spin_unlock_irqrestore(&ui->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ui->lock, flags);
	return 0;
}

static int
usb_ept_set_halt(struct usb_ep *_ep, int value)
{
	struct msm_endpoint *ept = to_msm_endpoint(_ep);
	struct usb_info *ui = ept->ui;
	unsigned int in = ept->flags & EPT_FLAG_IN;
	unsigned int n;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);

	n = readl(USB_ENDPTCTRL(ept->num));

	if (in) {
		if (value)
			n |= CTRL_TXS;
		else {
			n &= ~CTRL_TXS;
			n |= CTRL_TXR;
		}
	} else {
		if (value)
			n |= CTRL_RXS;
		else {
			n &= ~CTRL_RXS;
			n |= CTRL_RXR;
		}
	}
	writel(n, USB_ENDPTCTRL(ept->num));
	if (!value)
		ept->wedged = 0;
	spin_unlock_irqrestore(&ui->lock, flags);

	return 0;
}

static int
msm72k_set_halt(struct usb_ep *_ep, int value)
{
	struct msm_endpoint *ept = to_msm_endpoint(_ep);
	unsigned int in = ept->flags & EPT_FLAG_IN;

	if (value && in && ept->req)
		return -EAGAIN;

	usb_ept_set_halt(_ep, value);

	return 0;
}

static int
msm72k_fifo_status(struct usb_ep *_ep)
{
	return -EOPNOTSUPP;
}

static void
msm72k_fifo_flush(struct usb_ep *_ep)
{
	flush_endpoint(to_msm_endpoint(_ep));
}
static int msm72k_set_wedge(struct usb_ep *_ep)
{
	struct msm_endpoint *ept = to_msm_endpoint(_ep);

	if (ept->num == 0)
		return -EINVAL;

	ept->wedged = 1;

	return msm72k_set_halt(_ep, 1);
}

static const struct usb_ep_ops msm72k_ep_ops = {
	.enable		= msm72k_enable,
	.disable	= msm72k_disable,

	.alloc_request	= msm72k_alloc_request,
	.free_request	= msm72k_free_request,

	.queue		= msm72k_queue,
	.dequeue	= msm72k_dequeue,

	.set_halt	= msm72k_set_halt,
	.set_wedge	= msm72k_set_wedge,
	.fifo_status	= msm72k_fifo_status,
	.fifo_flush	= msm72k_fifo_flush,
};

static int msm72k_get_frame(struct usb_gadget *_gadget)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);

	/* frame number is in bits 13:3 */
	return (readl(USB_FRINDEX) >> 3) & 0x000007FF;
}

/* VBUS reporting logically comes from a transceiver */
static int msm72k_udc_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);

	if (is_active || atomic_read(&otg->chg_type)
					 == USB_CHG_TYPE__WALLCHARGER)
		wake_lock(&ui->wlock);

	msm_hsusb_set_vbus_state(is_active);
	return 0;
}

/* SW workarounds
Issue #1	- USB Spoof Disconnect Failure
Symptom	- Writing 0 to run/stop bit of USBCMD doesn't cause disconnect
SW workaround	- Making opmode non-driving and SuspendM set in function
		register of SMSC phy
*/
/* drivers may have software control over D+ pullup */
static int msm72k_pullup_internal(struct usb_gadget *_gadget, int is_active)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	unsigned long flags;

	if (is_active) {
		spin_lock_irqsave(&ui->lock, flags);
		if (is_usb_online(ui) && ui->driver)
			writel(readl(USB_USBCMD) | USBCMD_RS, USB_USBCMD);
		spin_unlock_irqrestore(&ui->lock, flags);
	} else {
		writel(readl(USB_USBCMD) & ~USBCMD_RS, USB_USBCMD);
		/* S/W workaround, Issue#1 */
		usb_phy_io_write(ui->xceiv, 0x48, 0x04);
	}

	/* Ensure pull-up operation is completed before returning */
	mb();

	return 0;
}

static int msm72k_pullup(struct usb_gadget *_gadget, int is_active)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	unsigned long flags;

	atomic_set(&ui->softconnect, is_active);

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->usb_state == USB_STATE_NOTATTACHED || ui->driver == NULL ||
		atomic_read(&otg->chg_type) == USB_CHG_TYPE__WALLCHARGER) {
		spin_unlock_irqrestore(&ui->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	msm72k_pullup_internal(_gadget, is_active);

	if (is_active && !ui->gadget.is_a_peripheral)
		schedule_delayed_work(&ui->chg_det, USB_CHG_DET_DELAY);

	return 0;
}

static int msm72k_wakeup(struct usb_gadget *_gadget)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	struct msm_otg *otg = to_msm_otg(ui->xceiv);

	if (!atomic_read(&ui->remote_wakeup)) {
		dev_err(&ui->pdev->dev,
			"%s: remote wakeup not supported\n", __func__);
		return -ENOTSUPP;
	}

	if (!atomic_read(&ui->configured)) {
		dev_err(&ui->pdev->dev,
			"%s: device is not configured\n", __func__);
		return -ENODEV;
	}
	usb_phy_set_suspend(ui->xceiv, 0);

	disable_irq(otg->irq);

	if (!is_usb_active())
		writel(readl(USB_PORTSC) | PORTSC_FPR, USB_PORTSC);

	/* Ensure that USB port is resumed before enabling the IRQ */
	mb();

	enable_irq(otg->irq);

	return 0;
}

/* when Gadget is configured, it will indicate how much power
 * can be pulled from vbus, as specified in configuiration descriptor
 */
static int msm72k_udc_vbus_draw(struct usb_gadget *_gadget, unsigned mA)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	unsigned long flags;


	spin_lock_irqsave(&ui->lock, flags);
	ui->b_max_pow = mA;
	ui->flags = USB_FLAG_CONFIGURED;
	spin_unlock_irqrestore(&ui->lock, flags);

	schedule_work(&ui->work);

	return 0;
}

static int msm72k_set_selfpowered(struct usb_gadget *_gadget, int set)
{
	struct usb_info *ui = container_of(_gadget, struct usb_info, gadget);
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ui->lock, flags);
	if (set) {
		if (ui->pdata && ui->pdata->self_powered)
			atomic_set(&ui->self_powered, 1);
		else
			ret = -EOPNOTSUPP;
	} else {
		/* We can always work as a bus powered device */
		atomic_set(&ui->self_powered, 0);
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	return ret;

}

static int msm72k_gadget_start(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *));
static int msm72k_gadget_stop(struct usb_gadget_driver *driver);


static const struct usb_gadget_ops msm72k_ops = {
	.get_frame	= msm72k_get_frame,
	.vbus_session	= msm72k_udc_vbus_session,
	.vbus_draw	= msm72k_udc_vbus_draw,
	.pullup		= msm72k_pullup,
	.wakeup		= msm72k_wakeup,
	.set_selfpowered = msm72k_set_selfpowered,
	.start		= msm72k_gadget_start,
	.stop		= msm72k_gadget_stop
};

static void usb_do_remote_wakeup(struct work_struct *w)
{
	struct usb_info *ui = the_usb_info;

	msm72k_wakeup(&ui->gadget);
}

static ssize_t usb_remote_wakeup(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_info *ui = the_usb_info;

	msm72k_wakeup(&ui->gadget);

	return count;
}

static ssize_t show_usb_state(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	size_t i;
	char *state[] = {"USB_STATE_NOTATTACHED", "USB_STATE_ATTACHED",
			"USB_STATE_POWERED", "USB_STATE_UNAUTHENTICATED",
			"USB_STATE_RECONNECTING", "USB_STATE_DEFAULT",
			"USB_STATE_ADDRESS", "USB_STATE_CONFIGURED",
			"USB_STATE_SUSPENDED"
	};

	i = scnprintf(buf, PAGE_SIZE, "%s\n", state[msm_hsusb_get_state()]);
	return i;
}

static ssize_t show_usb_speed(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_info *ui = the_usb_info;
	size_t i;
	char *speed[] = {"USB_SPEED_UNKNOWN", "USB_SPEED_LOW",
			"USB_SPEED_FULL", "USB_SPEED_HIGH"};

	i = scnprintf(buf, PAGE_SIZE, "%s\n", speed[ui->gadget.speed]);
	return i;
}

static ssize_t store_usb_chg_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_info *ui = the_usb_info;
	unsigned long mA;

	if (ui->gadget.is_a_peripheral)
		return -EINVAL;

	if (strict_strtoul(buf, 10, &mA))
		return -EINVAL;

	ui->chg_current = mA;
	usb_phy_set_power(ui->xceiv, mA);

	return count;
}

static ssize_t show_usb_chg_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_info *ui = the_usb_info;
	size_t count;

	count = snprintf(buf, PAGE_SIZE, "%d", ui->chg_current);

	return count;
}

static ssize_t show_usb_chg_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_info *ui = the_usb_info;
	struct msm_otg *otg = to_msm_otg(ui->xceiv);
	size_t count;
	char *chg_type[] = {"STD DOWNSTREAM PORT",
			"CARKIT",
			"DEDICATED CHARGER",
			"INVALID"};

	count = snprintf(buf, PAGE_SIZE, "%s",
			chg_type[atomic_read(&otg->chg_type)]);

	return count;
}
static DEVICE_ATTR(wakeup, S_IWUSR, 0, usb_remote_wakeup);
static DEVICE_ATTR(usb_state, S_IRUSR, show_usb_state, 0);
static DEVICE_ATTR(usb_speed, S_IRUSR, show_usb_speed, 0);
static DEVICE_ATTR(chg_type, S_IRUSR, show_usb_chg_type, 0);
static DEVICE_ATTR(chg_current, S_IWUSR | S_IRUSR,
		show_usb_chg_current, store_usb_chg_current);

#ifdef CONFIG_USB_OTG
static ssize_t store_host_req(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct usb_info *ui = the_usb_info;
	unsigned long val, flags;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	dev_dbg(&ui->pdev->dev, "%s host request\n",
			val ? "set" : "clear");

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->hnp_avail)
		ui->gadget.host_request = !!val;
	spin_unlock_irqrestore(&ui->lock, flags);

	return count;
}
static DEVICE_ATTR(host_request, S_IWUSR, NULL, store_host_req);

/* How do we notify user space about HNP availability?
 * As we are compliant to Rev 2.0, Host will not set a_hnp_support.
 * Introduce hnp_avail flag and set when HNP polling request arrives.
 * The expectation is that user space checks hnp availability before
 * requesting host role via above sysfs node.
 */
static ssize_t show_host_avail(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_info *ui = the_usb_info;
	size_t count;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	count = snprintf(buf, PAGE_SIZE, "%d\n", ui->hnp_avail);
	spin_unlock_irqrestore(&ui->lock, flags);

	return count;
}
static DEVICE_ATTR(host_avail, S_IRUSR, show_host_avail, NULL);

static struct attribute *otg_attrs[] = {
	&dev_attr_host_request.attr,
	&dev_attr_host_avail.attr,
	NULL,
};

static struct attribute_group otg_attr_grp = {
	.name  = "otg",
	.attrs = otg_attrs,
};
#endif

static int msm72k_probe(struct platform_device *pdev)
{
	struct usb_info *ui;
	struct msm_otg *otg;
	int retval;

	dev_dbg(&pdev->dev, "msm72k_probe\n");
	ui = kzalloc(sizeof(struct usb_info), GFP_KERNEL);
	if (!ui)
		return -ENOMEM;

	ui->pdev = pdev;
	ui->pdata = pdev->dev.platform_data;

	ui->buf = dma_alloc_coherent(&pdev->dev, 4096, &ui->dma, GFP_KERNEL);
	if (!ui->buf)
		return usb_free(ui, -ENOMEM);

	ui->pool = dma_pool_create("msm72k_udc", NULL, 32, 32, 0);
	if (!ui->pool)
		return usb_free(ui, -ENOMEM);

	ui->xceiv = usb_get_transceiver();
	if (!ui->xceiv)
		return usb_free(ui, -ENODEV);

	otg = to_msm_otg(ui->xceiv);
	ui->addr = otg->regs;

	ui->gadget.ops = &msm72k_ops;
	ui->gadget.max_speed = USB_SPEED_HIGH;
	device_initialize(&ui->gadget.dev);
	dev_set_name(&ui->gadget.dev, "gadget");
	ui->gadget.dev.parent = &pdev->dev;
	ui->gadget.dev.dma_mask = pdev->dev.dma_mask;

#ifdef CONFIG_USB_OTG
	ui->gadget.is_otg = 1;
#endif

	retval = usb_add_gadget_udc(&pdev->dev, &ui->gadget);
	if (retval)
		return usb_free(ui, retval);

	ui->sdev.name = DRIVER_NAME;
	ui->sdev.print_name = print_switch_name;
	ui->sdev.print_state = print_switch_state;

	retval = switch_dev_register(&ui->sdev);
	if (retval)
		return usb_free(ui, retval);

	the_usb_info = ui;

	wake_lock_init(&ui->wlock,
			WAKE_LOCK_SUSPEND, "usb_bus_active");

	usb_debugfs_init(ui);

	usb_prepare(ui);

#ifdef CONFIG_USB_OTG
	retval = sysfs_create_group(&pdev->dev.kobj, &otg_attr_grp);
	if (retval) {
		dev_err(&ui->pdev->dev,
			"failed to create otg sysfs directory:"
			"err:(%d)\n", retval);
	}
#endif

	retval = otg_set_peripheral(ui->xceiv->otg, &ui->gadget);
	if (retval) {
		dev_err(&ui->pdev->dev,
			"%s: Cannot bind the transceiver, retval:(%d)\n",
			__func__, retval);
		switch_dev_unregister(&ui->sdev);
		wake_lock_destroy(&ui->wlock);
		return usb_free(ui, retval);
	}

	pm_runtime_enable(&pdev->dev);

	/* Setup phy stuck timer */
	if (ui->pdata && ui->pdata->is_phy_status_timer_on)
		setup_timer(&phy_status_timer, usb_phy_status_check_timer, 0);
	return 0;
}

static int msm72k_gadget_start(struct usb_gadget_driver *driver,
			    int (*bind)(struct usb_gadget *))
{
	struct usb_info *ui = the_usb_info;
	int			retval, n;

	if (!driver
			|| driver->max_speed < USB_SPEED_FULL
			|| !bind
			|| !driver->disconnect
			|| !driver->setup)
		return -EINVAL;
	if (!ui)
		return -ENODEV;
	if (ui->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	ui->driver = driver;
	ui->gadget.dev.driver = &driver->driver;
	ui->gadget.name = driver_name;
	INIT_LIST_HEAD(&ui->gadget.ep_list);
	ui->gadget.ep0 = &ui->ep0in.ep;
	INIT_LIST_HEAD(&ui->gadget.ep0->ep_list);
	ui->gadget.speed = USB_SPEED_UNKNOWN;
	atomic_set(&ui->softconnect, 1);

	for (n = 1; n < 16; n++) {
		struct msm_endpoint *ept = ui->ept + n;
		list_add_tail(&ept->ep.ep_list, &ui->gadget.ep_list);
		ept->ep.maxpacket = 512;
	}
	for (n = 17; n < 32; n++) {
		struct msm_endpoint *ept = ui->ept + n;
		list_add_tail(&ept->ep.ep_list, &ui->gadget.ep_list);
		ept->ep.maxpacket = 512;
	}

	retval = device_add(&ui->gadget.dev);
	if (retval)
		goto fail;

	retval = bind(&ui->gadget);
	if (retval) {
		dev_err(&ui->pdev->dev, "bind to driver %s --> error %d\n",
				driver->driver.name, retval);
		device_del(&ui->gadget.dev);
		goto fail;
	}

	retval = device_create_file(&ui->gadget.dev, &dev_attr_wakeup);
	if (retval != 0)
		dev_err(&ui->pdev->dev, "failed to create sysfs entry:"
			"(wakeup) error: (%d)\n", retval);
	retval = device_create_file(&ui->gadget.dev, &dev_attr_usb_state);
	if (retval != 0)
		dev_err(&ui->pdev->dev, "failed to create sysfs entry:"
			" (usb_state) error: (%d)\n", retval);

	retval = device_create_file(&ui->gadget.dev, &dev_attr_usb_speed);
	if (retval != 0)
		dev_err(&ui->pdev->dev, "failed to create sysfs entry:"
			" (usb_speed) error: (%d)\n", retval);

	retval = device_create_file(&ui->gadget.dev, &dev_attr_chg_type);
	if (retval != 0)
		dev_err(&ui->pdev->dev,
			"failed to create sysfs entry(chg_type): err:(%d)\n",
					retval);
	retval = device_create_file(&ui->gadget.dev, &dev_attr_chg_current);
	if (retval != 0)
		dev_err(&ui->pdev->dev,
			"failed to create sysfs entry(chg_current):"
			"err:(%d)\n", retval);

	dev_dbg(&ui->pdev->dev, "registered gadget driver '%s'\n",
			driver->driver.name);
	usb_start(ui);

	return 0;

fail:
	ui->driver = NULL;
	ui->gadget.dev.driver = NULL;
	return retval;
}

static int msm72k_gadget_stop(struct usb_gadget_driver *driver)
{
	struct usb_info *dev = the_usb_info;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver || !driver->unbind)
		return -EINVAL;

	msm72k_pullup_internal(&dev->gadget, 0);
	if (dev->irq) {
		free_irq(dev->irq, dev);
		dev->irq = 0;
	}
	dev->state = USB_STATE_IDLE;
	atomic_set(&dev->configured, 0);
	switch_set_state(&dev->sdev, 0);
	/* cancel pending ep0 transactions */
	flush_endpoint(&dev->ep0out);
	flush_endpoint(&dev->ep0in);

	device_remove_file(&dev->gadget.dev, &dev_attr_wakeup);
	device_remove_file(&dev->gadget.dev, &dev_attr_usb_state);
	device_remove_file(&dev->gadget.dev, &dev_attr_usb_speed);
	device_remove_file(&dev->gadget.dev, &dev_attr_chg_type);
	device_remove_file(&dev->gadget.dev, &dev_attr_chg_current);
	driver->disconnect(&dev->gadget);
	driver->unbind(&dev->gadget);
	dev->gadget.dev.driver = NULL;
	dev->driver = NULL;

	device_del(&dev->gadget.dev);

	dev_dbg(&dev->pdev->dev,
		"unregistered gadget driver '%s'\n", driver->driver.name);
	return 0;
}


static int msm72k_udc_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int msm72k_udc_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static int msm72k_udc_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: idling...\n");
	return 0;
}

static struct dev_pm_ops msm72k_udc_dev_pm_ops = {
	.runtime_suspend = msm72k_udc_runtime_suspend,
	.runtime_resume = msm72k_udc_runtime_resume,
	.runtime_idle = msm72k_udc_runtime_idle
};

static int __exit msm72k_remove(struct platform_device *pdev)
{
	struct usb_info *ui = container_of(&pdev, struct usb_info, pdev);

	return usb_free(ui, 0);
}

static struct platform_driver usb_driver = {
	.probe = msm72k_probe,
	.remove = msm72k_remove,
	.driver = { .name = "msm_hsusb",
		    .pm = &msm72k_udc_dev_pm_ops, },
};

static int __init init(void)
{
	return platform_driver_register(&usb_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	platform_driver_unregister(&usb_driver);
}
module_exit(cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Mike Lockwood, Brian Swetland");
MODULE_LICENSE("GPL");
