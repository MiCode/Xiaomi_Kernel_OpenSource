/* drivers/usb/function/msm_hsusb.c
 *
 * Driver for HighSpeed USB Client Controller in MSM7K
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/switch.h>

#include <linux/usb/ch9.h>
#include <linux/io.h>

#include <asm/mach-types.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/gpio.h>
#include <mach/msm_hsusb_hw.h>
#include <mach/msm_otg.h>
#include <linux/wakelock.h>
#include <linux/pm_qos_params.h>
#include <mach/clk.h>

#define MSM_USB_BASE ((unsigned) ui->addr)

#include "usb_function.h"

#define EPT_FLAG_IN	0x0001
#define USB_DIR_MASK	USB_DIR_IN
#define SETUP_BUF_SIZE	4096

/* IDs for string descriptors */
#define STRING_LANGUAGE_ID      0
#define STRING_SERIAL           1
#define STRING_PRODUCT          2
#define STRING_MANUFACTURER     3

#define LANGUAGE_ID             0x0409 /* en-US */
#define SOC_ROC_2_0		0x10002 /* ROC 2.0 */

#define TRUE			1
#define FALSE			0
#define USB_LINK_RESET_TIMEOUT	(msecs_to_jiffies(10))
#define USB_CHG_DET_DELAY	msecs_to_jiffies(1000)

#define is_phy_45nm()     (PHY_MODEL(ui->phy_info) == USB_PHY_MODEL_45NM)
#define is_phy_external() (PHY_TYPE(ui->phy_info) == USB_PHY_EXTERNAL)

static int pid = 0x9018;

struct usb_fi_ept {
	struct usb_endpoint *ept;
	struct usb_endpoint_descriptor desc;
};

struct usb_function_info {
	struct list_head list;
	unsigned enabled;
	struct usb_function *func;
};

struct msm_request {
	struct usb_request req;

	struct usb_info *ui;
	struct msm_request *next;

	unsigned busy:1;
	unsigned live:1;
	unsigned alloced:1;
	unsigned dead:1;

	dma_addr_t dma;

	struct ept_queue_item *item;
	dma_addr_t item_dma;
};
static unsigned char str_lang_desc[] = {4,
				USB_DT_STRING,
				(unsigned char)LANGUAGE_ID,
				(unsigned char)(LANGUAGE_ID >> 8)};

#define to_msm_request(r) container_of(r, struct msm_request, req)
static int usb_hw_reset(struct usb_info *ui);
static void usb_vbus_online(struct usb_info *);
static void usb_vbus_offline(struct usb_info *ui);
static void usb_lpm_exit(struct usb_info *ui);
static void usb_lpm_wakeup_phy(struct work_struct *);
static void usb_exit(void);
static int usb_is_online(struct usb_info *ui);
static void usb_do_work(struct work_struct *w);
static int usb_lpm_enter(struct usb_info *ui);
int (*usb_lpm_config_gpio)(int);
static void usb_enable_pullup(struct usb_info *ui);
static void usb_disable_pullup(struct usb_info *ui);

static struct workqueue_struct *usb_work;
static void usb_chg_stop(struct work_struct *w);

#define USB_STATE_IDLE    0
#define USB_STATE_ONLINE  1
#define USB_STATE_OFFLINE 2

#define USB_FLAG_START          0x0001
#define USB_FLAG_VBUS_ONLINE    0x0002
#define USB_FLAG_VBUS_OFFLINE   0x0004
#define USB_FLAG_RESET          0x0008
#define USB_FLAG_SUSPEND	0x0010
#define USB_FLAG_CONFIGURE	0x0020
#define USB_FLAG_RESUME	0x0040
#define USB_FLAG_REG_OTG 0x0080

#define USB_MSC_ONLY_FUNC_MAP	0x10
#define DRIVER_NAME		"msm_hsusb_peripheral"

struct lpm_info {
	struct work_struct wakeup_phy;
};

enum charger_type {
	USB_CHG_TYPE__SDP,
	USB_CHG_TYPE__CARKIT,
	USB_CHG_TYPE__WALLCHARGER,
	USB_CHG_TYPE__INVALID
};

struct usb_info {
	/* lock for register/queue/device state changes */
	spinlock_t lock;

	/* single request used for handling setup transactions */
	struct usb_request *setup_req;
	struct usb_request *ep0out_req;

	struct platform_device *pdev;
	struct msm_hsusb_platform_data *pdata;
	int irq;
	int gpio_irq[2];
	void *addr;

	unsigned state;
	unsigned flags;

	unsigned online;
	unsigned running;
	unsigned bound;

	struct dma_pool *pool;

	/* dma page to back the queue heads and items */
	unsigned char *buf;
	dma_addr_t dma;

	struct ept_queue_head *head;

	/* used for allocation */
	unsigned next_item;
	unsigned next_ifc_num;
	unsigned stopped:1;
	unsigned remote_wakeup:1;
	unsigned configured:1;
	unsigned selfpowered:1;
	unsigned iad:1;
	unsigned char maxpower;
	enum usb_device_speed speed;
	unsigned phy_info;

	/* endpoints are ordered based on their status bits,
	** so they are OUT0, OUT1, ... OUT15, IN0, IN1, ... IN15
	*/
	struct usb_endpoint ept[32];

	struct delayed_work work;
	struct delayed_work chg_legacy_det;
	unsigned phy_status;
	unsigned phy_fail_count;
	struct usb_composition *composition;

	struct usb_function_info **func;
	unsigned num_funcs;
	struct usb_function_map *functions_map;

#define MAX_INTERFACE_NUM	15
	struct usb_function *func2ifc_map[MAX_INTERFACE_NUM];

#define ep0out ept[0]
#define ep0in  ept[16]

	struct clk *clk;
	struct clk *pclk;
	struct clk *cclk;
	unsigned int clk_enabled;

	struct vreg *vreg;
	unsigned int vreg_enabled;

	unsigned in_lpm;
	struct lpm_info li;

	enum charger_type chg_type;
	struct work_struct chg_stop;
#define MAX_STRDESC_NUM		100
	char **strdesc;
	int strdesc_index;

	u16 test_mode;
	struct wake_lock wlock;
	struct msm_otg_transceiver *xceiv;
	int active;
	enum usb_device_state usb_state;
	int vbus_sn_notif;
	struct switch_dev sdev;
};
static struct usb_info *the_usb_info;

static unsigned short usb_validate_product_id(unsigned short pid);
static unsigned short usb_get_product_id(unsigned long enabled_functions);
static void usb_switch_composition(unsigned short pid);
static unsigned short usb_set_composition(unsigned short pid);
static void usb_configure_device_descriptor(struct usb_info *ui);
static void usb_uninit(struct usb_info *ui);

static unsigned ulpi_read(struct usb_info *ui, unsigned reg);
static int ulpi_write(struct usb_info *ui, unsigned val, unsigned reg);



struct usb_device_descriptor desc_device = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	/* the following fields are filled in by usb_probe */
	.idVendor = 0,
	.idProduct = 0,
	.bcdDevice = 0,
	.iManufacturer = 0,
	.iProduct = 0,
	.iSerialNumber = 0,
	.bNumConfigurations = 1,
};

static void flush_endpoint(struct usb_endpoint *ept);
static void msm_hsusb_suspend_locks_acquire(struct usb_info *, int);

static ssize_t print_switch_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", DRIVER_NAME);
}

static ssize_t print_switch_state(struct switch_dev *sdev, char *buf)
{
	struct usb_info *ui = the_usb_info;

	return sprintf(buf, "%s\n", (ui->online ? "online" : "offline"));
}

#define USB_WALLCHARGER_CHG_CURRENT 1800
static int usb_get_max_power(struct usb_info *ui)
{
	unsigned long flags;
	enum charger_type temp;
	int suspended;
	int configured;

	spin_lock_irqsave(&ui->lock, flags);
	temp = ui->chg_type;
	suspended = ui->usb_state == USB_STATE_SUSPENDED ? 1 : 0;
	configured = ui->configured;
	spin_unlock_irqrestore(&ui->lock, flags);

	if (temp == USB_CHG_TYPE__INVALID)
		return -ENODEV;

	if (temp == USB_CHG_TYPE__WALLCHARGER)
		return USB_WALLCHARGER_CHG_CURRENT;

	if (suspended || !configured)
		return 0;

	return ui->maxpower * 2;
}

static void usb_chg_legacy_detect(struct work_struct *w)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;
	enum charger_type temp = USB_CHG_TYPE__INVALID;
	int maxpower;
	int ret = 0;

	spin_lock_irqsave(&ui->lock, flags);

	if (ui->usb_state == USB_STATE_NOTATTACHED) {
		ret = -ENODEV;
		goto chg_legacy_det_out;
	}

	if ((readl(USB_PORTSC) & PORTSC_LS) == PORTSC_LS) {
		ui->chg_type = temp = USB_CHG_TYPE__WALLCHARGER;
		goto chg_legacy_det_out;
	}

	ui->chg_type = temp = USB_CHG_TYPE__SDP;
chg_legacy_det_out:
	spin_unlock_irqrestore(&ui->lock, flags);

	if (ret)
		return;

	msm_chg_usb_charger_connected(temp);
	maxpower = usb_get_max_power(ui);
	if (maxpower > 0)
		msm_chg_usb_i_is_available(maxpower);

	/* USB driver prevents idle and suspend power collapse(pc)
	 * while usb cable is connected. But when dedicated charger is
	 * connected, driver can vote for idle and suspend pc. In order
	 * to allow pc, driver has to initiate low power mode which it
	 * cannot do as phy cannot be accessed when dedicated charger
	 * is connected due to phy lockup issues. Just to allow idle &
	 * suspend pc when dedicated charger is connected, release the
	 * wakelock, set driver latency to default and act as if we are
	 * in low power mode so that, driver will re-acquire wakelocks
	 * for any sub-sequent usb interrupts.
	 */
	if (temp == USB_CHG_TYPE__WALLCHARGER) {
		pr_info("\n%s: WALL-CHARGER\n", __func__);
		spin_lock_irqsave(&ui->lock, flags);
		if (ui->usb_state == USB_STATE_NOTATTACHED) {
			spin_unlock_irqrestore(&ui->lock, flags);
			return;
		}
		ui->in_lpm = 1;
		spin_unlock_irqrestore(&ui->lock, flags);

		msm_hsusb_suspend_locks_acquire(ui, 0);
	} else
		pr_info("\n%s: Standard Downstream Port\n", __func__);
}

int usb_msm_get_next_strdesc_id(char *str)
{
	struct usb_info *ui = the_usb_info;
	unsigned id;
	unsigned long flags;
	int len;

	len = strlen(str);
	if (!len) {
		printk(KERN_ERR "usb next_strdesc_id(); null string\n");
		return -EPERM;
	}
	/* for null character */
	len = len + 1;

	spin_lock_irqsave(&ui->lock, flags);

	id = ui->strdesc_index;
	if (id >= MAX_STRDESC_NUM) {
		id = -EPERM;
		printk(KERN_ERR "reached max strdesc number\n");
		goto get_strd_id_exit;
	}

	ui->strdesc[id] = kmalloc(len, GFP_ATOMIC);
	if (ui->strdesc[id]) {
		memcpy(ui->strdesc[id], str, len);
		ui->strdesc_index++;
	} else {
		id = -EPERM;
		printk(KERN_ERR "usb next_strdesc_id(); Out of memory:(%s)\n",
			str);
	}

get_strd_id_exit:
	spin_unlock_irqrestore(&ui->lock, flags);
	return id;
}
EXPORT_SYMBOL(usb_msm_get_next_strdesc_id);


inline int usb_msm_is_iad(void)
{
	return the_usb_info->iad;
}
EXPORT_SYMBOL(usb_msm_is_iad);

inline void usb_msm_enable_iad(void)
{
	the_usb_info->iad = 1;
}
EXPORT_SYMBOL(usb_msm_enable_iad);

int usb_msm_get_speed()
{
	return the_usb_info->speed;
}
EXPORT_SYMBOL(usb_msm_get_speed);

int usb_msm_get_next_ifc_number(struct usb_function *driver)
{
	struct usb_info *ui = the_usb_info;
	int ifc_num = -1;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ui->lock, flags);
	for (i = 0; i < ui->pdata->num_functions; i++) {
		if (strcmp(ui->functions_map[i].name, driver->name))
			continue;
		if (!(ui->composition->functions & (1 << i)))
			continue;
		ifc_num = ui->next_ifc_num++;
		ui->func2ifc_map[ifc_num] = driver;
		break;
	}
	spin_unlock_irqrestore(&ui->lock, flags);
	return ifc_num;
}
EXPORT_SYMBOL(usb_msm_get_next_ifc_number);

static inline int usb_msm_get_selfpowered(void)
{
	struct usb_info *ui = the_usb_info;

	return ui->selfpowered;
}
static inline int usb_msm_get_remotewakeup(void)
{
	struct usb_info *ui = the_usb_info;

	return ui->remote_wakeup;
}

static void usb_clk_enable(struct usb_info *ui)
{
	if (!ui->clk_enabled) {
		clk_enable(ui->pclk);
		if (ui->cclk)
			clk_enable(ui->cclk);
		ui->clk_enabled = 1;
	}
}

static void usb_clk_disable(struct usb_info *ui)
{
	if (ui->clk_enabled) {
		clk_disable(ui->pclk);
		if (ui->cclk)
			clk_disable(ui->cclk);
		ui->clk_enabled = 0;
	}
}

static void usb_vreg_enable(struct usb_info *ui)
{
	if (ui->vreg && !IS_ERR(ui->vreg) && !ui->vreg_enabled) {
		vreg_enable(ui->vreg);
		ui->vreg_enabled = 1;
	}
}

static void usb_vreg_disable(struct usb_info *ui)
{
	if (ui->vreg && !IS_ERR(ui->vreg) && ui->vreg_enabled) {
		vreg_disable(ui->vreg);
		ui->vreg_enabled = 0;
	}
}

static unsigned ulpi_read(struct usb_info *ui, unsigned reg)
{
	unsigned timeout = 100000;

	/* initiate read operation */
	writel(ULPI_RUN | ULPI_READ | ULPI_ADDR(reg),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while ((readl(USB_ULPI_VIEWPORT) & ULPI_RUN) && (--timeout)) ;

	if (timeout == 0) {
		printk(KERN_ERR "ulpi_read: timeout %08x\n",
			readl(USB_ULPI_VIEWPORT));
		return 0xffffffff;
	}
	return ULPI_DATA_READ(readl(USB_ULPI_VIEWPORT));
}

static int ulpi_write(struct usb_info *ui, unsigned val, unsigned reg)
{
	unsigned timeout = 10000;

	/* initiate write operation */
	writel(ULPI_RUN | ULPI_WRITE |
	       ULPI_ADDR(reg) | ULPI_DATA(val),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while((readl(USB_ULPI_VIEWPORT) & ULPI_RUN) && (--timeout)) ;

	if (timeout == 0) {
		printk(KERN_ERR "ulpi_write: timeout\n");
		return -1;
	}

	return 0;
}

static void msm_hsusb_suspend_locks_acquire(struct usb_info *ui, int acquire)
{
	if (acquire) {
		wake_lock(&ui->wlock);
		pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY,
				DRIVER_NAME, ui->pdata->swfi_latency);
		/* targets like 7x30 have introduced core clock
		 * to remove the dependency on max axi frequency
		 */
		if (!ui->cclk)
			pm_qos_update_requirement(PM_QOS_SYSTEM_BUS_FREQ,
					DRIVER_NAME, MSM_AXI_MAX_FREQ);
	} else {
		wake_lock_timeout(&ui->wlock, HZ / 2);
		pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY,
					DRIVER_NAME,
					PM_QOS_DEFAULT_VALUE);
		if (!ui->cclk)
			pm_qos_update_requirement(PM_QOS_SYSTEM_BUS_FREQ,
					DRIVER_NAME, PM_QOS_DEFAULT_VALUE);
	}
}

static void msm_hsusb_suspend_locks_init(struct usb_info *ui, int init)
{
	if (init) {
		wake_lock_init(&ui->wlock, WAKE_LOCK_SUSPEND,
				"usb_bus_active");
		pm_qos_add_requirement(PM_QOS_CPU_DMA_LATENCY,
				DRIVER_NAME,
				PM_QOS_DEFAULT_VALUE);
		pm_qos_add_requirement(PM_QOS_SYSTEM_BUS_FREQ,
				DRIVER_NAME, PM_QOS_DEFAULT_VALUE);
	} else {
		wake_lock_destroy(&ui->wlock);
		pm_qos_remove_requirement(PM_QOS_CPU_DMA_LATENCY, DRIVER_NAME);
		pm_qos_remove_requirement(PM_QOS_SYSTEM_BUS_FREQ, DRIVER_NAME);
	}
}

static void init_endpoints(struct usb_info *ui)
{
	unsigned n;

	for (n = 0; n < 32; n++) {
		struct usb_endpoint *ept = ui->ept + n;

		ept->ui = ui;
		ept->bit = n;
		ept->num = n & 15;
		ept->alloced = 0;

		if (ept->bit > 15) {
			/* IN endpoint */
			ept->head = ui->head + (ept->num << 1) + 1;
			ept->flags = EPT_FLAG_IN;
		} else {
			/* OUT endpoint */
			ept->head = ui->head + (ept->num << 1);
			ept->flags = 0;
		}
	}
}

void usb_configure_endpoint(struct usb_endpoint *ep,
			struct usb_endpoint_descriptor *ep_desc)
{
	unsigned cfg = 0;
	unsigned long flags;
	struct usb_info *ui = ep->ui;

	if (!ui)
		return;
	spin_lock_irqsave(&ui->lock, flags);

	if (ep_desc) {
		ep->max_pkt = ep_desc->wMaxPacketSize;
		ep->ep_descriptor = ep_desc;
	}

	if (!ep->max_pkt) {
		printk(KERN_ERR "cannot configure zero length max pkt\n");
		goto cfg_ept_end;
	}

	cfg = CONFIG_MAX_PKT(ep->max_pkt) | CONFIG_ZLT;
	/* ep0 out needs interrupt-on-setup */
	if (ep->bit == 0)
		cfg |= CONFIG_IOS;
	ep->head->config = cfg;
	ep->head->next = TERMINATE;

	pr_debug("ept #%d %s max:%d head:%p bit:%d\n",
		       ep->num,
		       (ep->flags & EPT_FLAG_IN) ? "in" : "out",
		       ep->max_pkt, ep->head, ep->bit);

cfg_ept_end:
	spin_unlock_irqrestore(&ui->lock, flags);
}
EXPORT_SYMBOL(usb_configure_endpoint);

#define NUM_EPTS 15	/* number of in or out non-ctrl endpoints */
struct usb_endpoint *usb_alloc_endpoint(unsigned direction)
{
	struct usb_info *ui = the_usb_info;
	struct usb_endpoint *ept = NULL;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	if (direction & USB_DIR_IN)
		ept = (&ui->ep0in);
	else
		ept = (&ui->ep0out);

	for (i = 0; i < NUM_EPTS; i++) {
		ept++;
		if (!ept->alloced) {
			ept->alloced = 1;
			ept->ui = ui;
			spin_unlock_irqrestore(&ui->lock, flags);
			return ept;
		}
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	return NULL;
}
EXPORT_SYMBOL(usb_alloc_endpoint);

int usb_free_endpoint(struct usb_endpoint *ept)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;

	if (!ept)
		return -EINVAL;
	spin_lock_irqsave(&ui->lock, flags);
	ept->alloced = 0;
	ept->ui = 0;
	spin_unlock_irqrestore(&ui->lock, flags);

	return 0;
}
EXPORT_SYMBOL(usb_free_endpoint);

struct usb_request *usb_ept_alloc_req(struct usb_endpoint *ept,
			unsigned bufsize)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req;

	if (!ui)
		return NULL;

	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		goto fail1;

	req->item = dma_pool_alloc(ui->pool, GFP_ATOMIC, &req->item_dma);
	if (!req->item)
		goto fail2;

	if (bufsize) {
		req->req.buf = kmalloc(bufsize, GFP_ATOMIC);
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
	return NULL;
}
EXPORT_SYMBOL(usb_ept_alloc_req);

static void do_free_req(struct usb_info *ui, struct msm_request *req)
{
	if (req->alloced)
		kfree(req->req.buf);

	dma_pool_free(ui->pool, req->item, req->item_dma);
	kfree(req);
}

void usb_ept_free_req(struct usb_endpoint *ept, struct usb_request *_req)
{
	struct msm_request *req, *temp_req, *prev_req;
	struct usb_info *ui;
	unsigned long flags;
	int dead = 0;
	if (!ept || !_req)
		return;

	ui = ept->ui;
	if (!ui)
		return;

	req = to_msm_request(_req);
	spin_lock_irqsave(&ui->lock, flags);
	/* defer freeing resources if request is still busy */
	if (req->busy)
		dead = req->dead = 1;
	spin_unlock_irqrestore(&ui->lock, flags);

	/* if req->dead, then we will clean up when the request finishes */
	if (!dead) {
		temp_req = ept->req;
		prev_req = temp_req;
		while (temp_req != NULL) {
			if (req == temp_req && ept->req != temp_req)
				prev_req->next = temp_req->next;

			prev_req = temp_req;
			temp_req = temp_req->next;
		}
		if (ept->req == req)
			ept->req = req->next;
		req->req.complete = NULL;
		do_free_req(ui, req);
	} else
		pr_err("%s: req is busy, can't free req\n", __func__);
}
EXPORT_SYMBOL(usb_ept_free_req);

void usb_ept_enable(struct usb_endpoint *ept, int yes)
{
	struct usb_info *ui;
	int in;
	unsigned n;
	unsigned char xfer;

	if (!ept || !ept->ui)
		return;
	ui = ept->ui;
	in = ept->flags & EPT_FLAG_IN;
	if (!ept->ep_descriptor)
		return;

	if (ui->in_lpm) {
		pr_err("%s: controller is in lpm, cannot proceed\n", __func__);
		return;
	}

	xfer = ept->ep_descriptor->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

	n = readl(USB_ENDPTCTRL(ept->num));

	if (in) {
		if (xfer == USB_ENDPOINT_XFER_BULK)
			n = (n & (~CTRL_TXT_MASK)) | CTRL_TXT_BULK;
		else if (xfer == USB_ENDPOINT_XFER_INT)
			n = (n & (~CTRL_TXT_MASK)) | CTRL_TXT_INT;
		if (yes)
			n |= CTRL_TXE | CTRL_TXR;
		else
			n &= (~CTRL_TXE);
	} else {
		if (xfer == USB_ENDPOINT_XFER_BULK)
			n = (n & (~CTRL_RXT_MASK)) | CTRL_RXT_BULK;
		else if (xfer == USB_ENDPOINT_XFER_INT)
			n = (n & (~CTRL_RXT_MASK)) | CTRL_RXT_INT;
		if (yes)
			n |= CTRL_RXE | CTRL_RXR;
		else
			n &= ~(CTRL_RXE);
	}
	/* complete all the updates to ept->head before enabling endpoint*/
	dma_coherent_pre_ops();
	writel(n, USB_ENDPTCTRL(ept->num));
}
EXPORT_SYMBOL(usb_ept_enable);

static void usb_ept_start(struct usb_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req = ept->req;

	BUG_ON(req->live);

	/* link the hw queue head to the request's transaction item */
	ept->head->next = req->item_dma;
	ept->head->info = 0;

	/* memory barrier to flush the data before priming endpoint*/
	dma_coherent_pre_ops();
	/* start the endpoint */
	writel(1 << ept->bit, USB_ENDPTPRIME);

	/* mark this chain of requests as live */
	while (req) {
		req->live = 1;
		if (req->item->next == TERMINATE)
			break;
		req = req->next;
	}
}

int usb_ept_queue_xfer(struct usb_endpoint *ept, struct usb_request *_req)
{
	unsigned long flags;
	struct msm_request *req = to_msm_request(_req);
	struct msm_request *last;
	struct usb_info *ui = ept->ui;
	struct ept_queue_item *item = req->item;
	unsigned length = req->req.length;

	if (length > 0x4000)
		return -EMSGSIZE;

	if (ui->in_lpm) {
		req->req.status = usb_remote_wakeup();
		if (req->req.status) {
			pr_debug("%s:RWakeup generation failed, EP = %x\n",
							__func__, ept->bit);
			return req->req.status;
		}
	}

	spin_lock_irqsave(&ui->lock, flags);

	if (req->busy) {
		req->req.status = -EBUSY;
		spin_unlock_irqrestore(&ui->lock, flags);
		printk(KERN_INFO
		       "usb_ept_queue_xfer() tried to queue busy request\n");
		return -EBUSY;
	}

	if (!ui->online && (ept->num != 0)) {
		req->req.status = -ENODEV;
		spin_unlock_irqrestore(&ui->lock, flags);
		printk(KERN_INFO "usb_ept_queue_xfer() tried to queue request"
				"while offline; ept->bit: %x\n", ept->bit);
		return -ENODEV;
	}

	req->busy = 1;
	req->live = 0;
	req->next = 0;
	req->req.status = -EBUSY;

	req->dma = dma_map_single(NULL, req->req.buf, length,
				  (ept->flags & EPT_FLAG_IN) ?
				  DMA_TO_DEVICE : DMA_FROM_DEVICE);

	/* prepare the transaction descriptor item for the hardware */
	item->next = TERMINATE;
	item->info = INFO_BYTES(length) | INFO_IOC | INFO_ACTIVE;
	item->page0 = req->dma;
	item->page1 = (req->dma + 0x1000) & 0xfffff000;
	item->page2 = (req->dma + 0x2000) & 0xfffff000;
	item->page3 = (req->dma + 0x3000) & 0xfffff000;

	/* Add the new request to the end of the queue */
	last = ept->last;
	if (last) {
		/* Already requests in the queue. add us to the
		 * end, but let the completion interrupt actually
		 * start things going, to avoid hw issues
		 */
		last->next = req;

		/* only modify the hw transaction next pointer if
		 * that request is not live
		 */
		if (!last->live)
			last->item->next = req->item_dma;
	} else {
		/* queue was empty -- kick the hardware */
		ept->req = req;
		usb_ept_start(ept);
	}
	ept->last = req;

	spin_unlock_irqrestore(&ui->lock, flags);
	return 0;
}
EXPORT_SYMBOL(usb_ept_queue_xfer);

int usb_ept_flush(struct usb_endpoint *ept)
{
	printk("usb_ept_flush \n");
	flush_endpoint(ept);
	return 0;
}

int usb_ept_get_max_packet(struct usb_endpoint *ept)
{
	return ept->max_pkt;
}
EXPORT_SYMBOL(usb_ept_get_max_packet);

int usb_remote_wakeup(void)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	if (!ui->remote_wakeup) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_err("%s: remote wakeup not supported\n", __func__);
		return -ENOTSUPP;
	}

	if (!ui->online) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_err("%s: device is not configured\n", __func__);
		return -ENODEV;
	}

	if (ui->in_lpm)
		usb_lpm_exit(ui);
	spin_unlock_irqrestore(&ui->lock, flags);

	/* if usb_lpm_exit is unable to set PHCD,
	 * it would initiate workthread to set the PHCD
	 */
	if (cancel_work_sync(&ui->li.wakeup_phy))
		usb_lpm_wakeup_phy(NULL);

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->in_lpm) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_err("%s: cannot bring controller out of lpm\n", __func__);
		return -ENODEV;
	}

	if (!usb_is_online(ui)) {
		pr_debug("%s: enabling force resume\n", __func__);
		writel(readl(USB_PORTSC) | PORTSC_FPR, USB_PORTSC);
	} else
		pr_debug("%s: controller seems to be out of suspend already\n",
				__func__);
	spin_unlock_irqrestore(&ui->lock, flags);

	return 0;
}
EXPORT_SYMBOL(usb_remote_wakeup);

/* --- endpoint 0 handling --- */

static void set_configuration(struct usb_info *ui, int yes)
{
	unsigned i;

	ui->online = !!yes;

	for (i = 0; i < ui->num_funcs; i++) {
		struct usb_function_info *fi = ui->func[i];
		if (!fi || !(ui->composition->functions & (1 << i)))
			continue;
		if (fi->func->configure)
			fi->func->configure(yes, fi->func->context);
	}
}

static void ep0out_complete(struct usb_endpoint *ept, struct usb_request *req)
{
	req->complete = 0;
}

static void ep0in_complete(struct usb_endpoint *ept, struct usb_request *req)
{
	/* queue up the receive of the ACK response from the host */
	if (req->status == 0) {
		struct usb_info *ui = ept->ui;
		req->length = 0;
		req->complete = ep0out_complete;
		usb_ept_queue_xfer(&ui->ep0out, req);
	}
}

static void ep0in_complete_sendzero(
		struct usb_endpoint *ept, struct usb_request *req)
{
	if (req->status == 0) {
		struct usb_info *ui = ept->ui;
		req->length = 0;
		req->complete = ep0in_complete;
		usb_ept_queue_xfer(&ui->ep0in, req);
	}
}

static void ep0_status_complete(
		struct usb_endpoint *ept, struct usb_request *req)
{
	struct usb_info *ui = ept->ui;
	unsigned int i;

	if (!ui->test_mode)
		return;

	switch (ui->test_mode) {
	case J_TEST:
		pr_info("usb electrical test mode: (J)\n");
		i = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(i | PORTSC_PTC_J_STATE, USB_PORTSC);
		break;

	case K_TEST:
		pr_info("usb electrical test mode: (K)\n");
		i = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(i | PORTSC_PTC_K_STATE, USB_PORTSC);
		break;

	case SE0_NAK_TEST:
		pr_info("usb electrical test mode: (SE0-NAK)\n");
		i = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(i | PORTSC_PTC_SE0_NAK, USB_PORTSC);
		break;

	case TST_PKT_TEST:
		pr_info("usb electrical test mode: (TEST_PKT)\n");
		i = readl(USB_PORTSC) & (~PORTSC_PTC);
		writel(i | PORTSC_PTC_TST_PKT, USB_PORTSC);
		break;
	default:
		pr_err("usb:%s: undefined test mode: (%x)\n",
				__func__, ui->test_mode);
	}

}

static void ep0_setup_ack(struct usb_info *ui)
{
	struct usb_request *req = ui->setup_req;
	req->length = 0;
	req->complete = ep0_status_complete;
	usb_ept_queue_xfer(&ui->ep0in, req);
}

static void ep0_setup_stall(struct usb_info *ui)
{
	writel((1<<16) | (1<<0), USB_ENDPTCTRL(0));
}

static void ep0_setup_receive(struct usb_info *ui, unsigned len)
{
	ui->ep0out_req->length = len;
	usb_ept_queue_xfer(&ui->ep0out, ui->ep0out_req);
}

static void ep0_setup_send(struct usb_info *ui, unsigned wlen)
{
	struct usb_request *req = ui->setup_req;
	struct usb_endpoint *ept = &ui->ep0in;

	/* never send more data than the host requested */
	if (req->length > wlen)
		req->length = wlen;

	/* if we are sending a short response that ends on
	 * a packet boundary, we'll need to send a zero length
	 * packet as well.
	 */
	if ((req->length != wlen) && ((req->length & 63) == 0)) {
		req->complete = ep0in_complete_sendzero;
	} else {
		req->complete = ep0in_complete;
	}

	usb_ept_queue_xfer(ept, req);
}


static int usb_find_descriptor(struct usb_info *ui, struct usb_ctrlrequest *ctl,
				struct usb_request *req);

static void handle_setup(struct usb_info *ui)
{
	struct usb_ctrlrequest ctl;

	memcpy(&ctl, ui->ep0out.head->setup_data, sizeof(ctl));
	writel(EPT_RX(0), USB_ENDPTSETUPSTAT);

	/* any pending ep0 transactions must be canceled */
	flush_endpoint(&ui->ep0out);
	flush_endpoint(&ui->ep0in);

	/* let functions handle vendor and class requests */
	if ((ctl.bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD) {
		struct usb_function *func;

		/* Send stall if received interface number is invalid */
		if (ctl.wIndex >= ui->next_ifc_num)
			goto stall;

		func = ui->func2ifc_map[ctl.wIndex];
		if (func && func->setup) {
			if (ctl.bRequestType & USB_DIR_IN) {
				struct usb_request *req = ui->setup_req;
				int ret = func->setup(&ctl,
						req->buf, SETUP_BUF_SIZE,
						func->context);
				if (ret >= 0) {
					req->length = ret;
					ep0_setup_send(ui, ctl.wLength);
					return;
				}
			} else {
				int ret = func->setup(&ctl, NULL, 0,
							func->context);
				if (ret == 0) {
					ep0_setup_ack(ui);
					return;
				} else if (ret > 0) {
					ep0_setup_receive(ui, ret);
					return;
				}
			}
		}
		goto stall;
		return;
	}

	switch (ctl.bRequest) {
	case USB_REQ_GET_STATUS:
	{
		struct usb_request *req = ui->setup_req;
		if ((ctl.bRequestType & (USB_DIR_MASK)) != (USB_DIR_IN))
			break;
		if (ctl.wLength != 2)
			break;
		req->length = 2;
		switch (ctl.bRequestType & USB_RECIP_MASK) {
		case USB_RECIP_ENDPOINT:
		{
			unsigned num = ctl.wIndex & USB_ENDPOINT_NUMBER_MASK;
			struct usb_endpoint *ept;

			if (num == 0)
				break;
			if (ctl.wIndex & USB_ENDPOINT_DIR_MASK)
				num += 16;
			ept = ui->ept + num;
			memcpy(req->buf, &ept->ept_halted, 2);
			break;
		}

		case USB_RECIP_DEVICE:
		{
			unsigned short temp = 0;
			if (usb_msm_get_selfpowered())
				temp = 1 << USB_DEVICE_SELF_POWERED;
			if (usb_msm_get_remotewakeup())
				temp |= 1 << USB_DEVICE_REMOTE_WAKEUP;
			memcpy(req->buf, &temp, 2);
			break;
		}

		case USB_RECIP_INTERFACE:
			memset(req->buf, 0, 2);
			break;
		default:
			printk(KERN_ERR "Unreconginized recipient\n");
			break;
		}

		ep0_setup_send(ui, 2);
		return;
	}

	case USB_REQ_GET_DESCRIPTOR:
	{
		struct usb_request *req;

		if ((ctl.bRequestType & (USB_DIR_MASK)) != (USB_DIR_IN))
			break;

		req = ui->setup_req;
		if (!usb_find_descriptor(ui, &ctl, req)) {
			if (req->length > ctl.wLength)
				req->length = ctl.wLength;
			ep0_setup_send(ui, ctl.wLength);
			return;
		}
		break;
	}

	case USB_REQ_SET_FEATURE:
		if ((ctl.bRequestType & (USB_DIR_MASK)) != (USB_DIR_OUT))
			break;
		if (ctl.wLength != 0)
			break;
		switch (ctl.bRequestType & USB_RECIP_MASK) {
		case USB_RECIP_DEVICE:
			if (ctl.wValue == USB_DEVICE_REMOTE_WAKEUP) {
				ui->remote_wakeup = 1;
				ep0_setup_ack(ui);
				return;
			} else if (ctl.wValue == USB_DEVICE_TEST_MODE) {
				if (ctl.wIndex & 0x0f)
					break;
				ui->test_mode = ctl.wIndex;
				ep0_setup_ack(ui);
				return;
			}
			break;

		case USB_RECIP_ENDPOINT:
		{
			unsigned num = ctl.wIndex & USB_ENDPOINT_NUMBER_MASK;
			if ((num == 0) || (ctl.wValue != 0))
				break;
			if (ctl.wIndex & USB_ENDPOINT_DIR_MASK)
				num += 16;
			usb_ept_set_halt(ui->ept + num);
			ep0_setup_ack(ui);
			return;
		}

		default:
			pr_err("usb: %s: set_feature: unrecognized recipient\n",
					__func__);
			break;
		}
		break;

	case USB_REQ_CLEAR_FEATURE:
	{
		if ((ctl.bRequestType & (USB_DIR_MASK)) != (USB_DIR_OUT))
			break;
		if (ctl.wLength != 0)
			break;

		switch (ctl.bRequestType & USB_RECIP_MASK) {
		case USB_RECIP_DEVICE:
			if (ctl.wValue != USB_DEVICE_REMOTE_WAKEUP)
				break;
			ui->remote_wakeup = 0;
			ep0_setup_ack(ui);
			return;
		case USB_RECIP_ENDPOINT:
		{
			unsigned num;
			if (ctl.wValue != USB_ENDPOINT_HALT)
				break;
			num = ctl.wIndex & USB_ENDPOINT_NUMBER_MASK;
			if (num != 0) {
				if (ctl.wIndex & USB_ENDPOINT_DIR_MASK)
					num += 16;
				usb_ept_clear_halt(ui->ept + num);
			}
			ep0_setup_ack(ui);
			return;
		}
		default:
			pr_info("unsupported clear feature command\n");
			pr_info("Request-type:(%08x) wValue:(%08x) "
					"wIndex:(%08x) wLength:(%08x)\n",
						ctl.bRequestType, ctl.wValue,
						ctl.wIndex, ctl.wLength);
			break;
		}
		break;
	}

	case USB_REQ_SET_INTERFACE:
		if ((ctl.bRequestType & (USB_DIR_MASK | USB_RECIP_MASK))
			!= (USB_DIR_OUT | USB_RECIP_INTERFACE))
			break;
		if (ui->func2ifc_map[ctl.wIndex]->set_interface) {
			ui->func2ifc_map[ctl.wIndex]->set_interface(ctl.wIndex,
					ctl.wValue,
					ui->func2ifc_map[ctl.wIndex]->context);
			ep0_setup_ack(ui);
			return;
		}
		break;
	case USB_REQ_GET_INTERFACE:
		{
		struct usb_function *f;
		struct usb_request *req = ui->setup_req;
		int ifc_num = ctl.wIndex;
		int ret = 0;

		if ((ctl.bRequestType & (USB_DIR_MASK | USB_RECIP_MASK))
					!= (USB_DIR_IN | USB_RECIP_INTERFACE))
			break;

		f = ui->func2ifc_map[ifc_num];
		if (!f->get_interface)
			break;
		ret = f->get_interface(ifc_num,
				ui->func2ifc_map[ifc_num]->context);
		if (ret < 0)
			break;
		req->length = ctl.wLength;
		memcpy(req->buf, &ret, req->length);
		ep0_setup_send(ui, ctl.wLength);
		return;
		}
	case USB_REQ_SET_CONFIGURATION:
		if ((ctl.bRequestType & USB_DIR_MASK) != USB_DIR_OUT)
			break;
		ui->configured = ctl.wValue;
		pr_info("hsusb set_configuration wValue = %d usbcmd = %x\n",
						ctl.wValue, readl(USB_USBCMD));
		set_configuration(ui, ctl.wValue);
		ep0_setup_ack(ui);
		ui->flags = USB_FLAG_CONFIGURE;
		if (ui->configured)
			ui->usb_state = USB_STATE_CONFIGURED;
		queue_delayed_work(usb_work, &ui->work, 0);
		return;

	case USB_REQ_GET_CONFIGURATION:
	{
		unsigned conf;
		struct usb_request *req = ui->setup_req;
		req->length = 1;
		conf = ui->configured;
		memcpy(req->buf, &conf, req->length);
		ep0_setup_send(ui, ctl.wLength);
		return;
	}

	case USB_REQ_SET_ADDRESS:
		if ((ctl.bRequestType & (USB_DIR_MASK | USB_RECIP_MASK))
			!= (USB_DIR_OUT | USB_RECIP_DEVICE))
			break;
		ui->usb_state = USB_STATE_ADDRESS;
		writel((ctl.wValue << 25) | (1 << 24), USB_DEVICEADDR);
		ep0_setup_ack(ui);
		return;
	}

stall:
	ep0_setup_stall(ui);
	return;

}

static void handle_endpoint(struct usb_info *ui, unsigned bit)
{
	struct usb_endpoint *ept = ui->ept + bit;
	struct msm_request *req;
	unsigned long flags;
	unsigned info;

#if 0
	printk(KERN_INFO "handle_endpoint() %d %s req=%p(%08x)\n",
	       ept->num, (ept->flags & EPT_FLAG_IN) ? "in" : "out",
	       ept->req, ept->req ? ept->req->item_dma : 0);
#endif
	if (!ept) {
		pr_err("%s: ept is null: ep bit = %d\n", __func__, bit);
		return;
	}

	/* expire all requests that are no longer active */
	spin_lock_irqsave(&ui->lock, flags);
	while ((req = ept->req)) {
		/* clean speculative fetches on req->item->info */
		dma_coherent_post_ops();
		info = req->item->info;

		/* if we've processed all live requests, time to
		 * restart the hardware on the next non-live request
		 */
		if (!req->live) {
			usb_ept_start(ept);
			break;
		}

		/* if the transaction is still in-flight, stop here */
		if (info & INFO_ACTIVE)
			break;

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
			printk(KERN_INFO "hsusb: ept %d %s error. info=%08x\n",
				ept->num,
				(ept->flags & EPT_FLAG_IN) ? "in" : "out",
			       info);
		} else {
			req->req.status = 0;
			req->req.actual = req->req.length - ((info >> 16) & 0x7FFF);
		}
		req->busy = 0;
		req->live = 0;
		if (req->dead)
			do_free_req(ui, req);

		if (req->req.complete) {
			spin_unlock_irqrestore(&ui->lock, flags);
			req->req.complete(ept, &req->req);
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

	if (ui->in_lpm) {
		pr_err("%s: controller is in lpm, cannot proceed\n", __func__);
		return;
	}

	do {
		writel(bits, USB_ENDPTFLUSH);
		while (readl(USB_ENDPTFLUSH) & bits)
			udelay(100);
	} while (readl(USB_ENDPTSTAT) & bits);
}

static void flush_endpoint_sw(struct usb_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	struct msm_request *req, *next;
	unsigned long flags;

	/* inactive endpoints have nothing to do here */
	if (!ui || !ept->alloced || !ept->max_pkt)
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
		next = req->next;

		req->busy = 0;
		req->live = 0;
		req->req.status = -ENODEV;
		req->req.actual = 0;
		if (req->req.complete) {
			spin_unlock_irqrestore(&ui->lock, flags);
			req->req.complete(ept, &req->req);
			spin_lock_irqsave(&ui->lock, flags);
		}
		if (req->dead)
			do_free_req(ui, req);
		req = req->next;
	}
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void flush_endpoint(struct usb_endpoint *ept)
{
	if (!ept->ui)
		return;

	flush_endpoint_hw(ept->ui, (1 << ept->bit));
	flush_endpoint_sw(ept);
}

static void flush_all_endpoints(struct usb_info *ui)
{
	unsigned n;

	flush_endpoint_hw(ui, 0xffffffff);

	for (n = 0; n < 32; n++)
		flush_endpoint_sw(ui->ept + n);
}

#define HW_DELAY_FOR_LPM msecs_to_jiffies(1000)
#define DELAY_FOR_USB_VBUS_STABILIZE msecs_to_jiffies(500)
static irqreturn_t usb_interrupt(int irq, void *data)
{
	struct usb_info *ui = data;
	unsigned n;
	unsigned speed;

	if (!ui->active)
		return IRQ_HANDLED;

	if (ui->in_lpm) {
		usb_lpm_exit(ui);
		return IRQ_HANDLED;
	}

	n = readl(USB_USBSTS);
	writel(n, USB_USBSTS);

	/* somehow we got an IRQ while in the reset sequence: ignore it */
	if (ui->running == 0) {
		pr_err("%s: ui->running is zero\n", __func__);
		return IRQ_HANDLED;
	}

	if (n & STS_PCI) {
		if (!(readl(USB_PORTSC) & PORTSC_PORT_RESET)) {
			speed = (readl(USB_PORTSC) & PORTSC_PORT_SPEED_MASK);
			switch (speed) {
			case PORTSC_PORT_SPEED_HIGH:
				pr_info("hsusb resume: speed = HIGH\n");
				ui->speed = USB_SPEED_HIGH;
				break;

			case PORTSC_PORT_SPEED_FULL:
				pr_info("hsusb resume: speed = FULL\n");
				ui->speed = USB_SPEED_FULL;
				break;

			default:
				pr_err("hsusb resume: Unknown Speed\n");
				ui->speed = USB_SPEED_UNKNOWN;
				break;
			}
		}

		/* pci interrutpt would also be generated when resuming
		 * from bus suspend, following check would avoid kick
		 * starting usb main thread in case of pci interrupts
		 * during enumeration
		 */
		if (ui->configured && ui->chg_type == USB_CHG_TYPE__SDP) {
			ui->usb_state = USB_STATE_CONFIGURED;
			ui->flags = USB_FLAG_RESUME;
			queue_delayed_work(usb_work, &ui->work, 0);
		}
	}

	if (n & STS_URI) {
		pr_info("hsusb reset interrupt\n");
		ui->usb_state = USB_STATE_DEFAULT;
		ui->configured = 0;
		schedule_work(&ui->chg_stop);

		writel(readl(USB_ENDPTSETUPSTAT), USB_ENDPTSETUPSTAT);
		writel(readl(USB_ENDPTCOMPLETE), USB_ENDPTCOMPLETE);
		writel(0xffffffff, USB_ENDPTFLUSH);
		writel(0, USB_ENDPTCTRL(1));

		if (ui->online != 0) {
			/* marking us offline will cause ept queue attempts to fail */
			ui->online = 0;

			flush_all_endpoints(ui);

			/* XXX: we can't seem to detect going offline, so deconfigure
			 * XXX: on reset for the time being
			 */
			set_configuration(ui, 0);
		}
	}

	if (n & STS_SLI) {
		pr_info("hsusb suspend interrupt\n");
		ui->usb_state = USB_STATE_SUSPENDED;

		/* stop usb charging */
		schedule_work(&ui->chg_stop);
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

	n = readl(USB_OTGSC);
	writel(n, USB_OTGSC);

	if (n & OTGSC_BSVIS) {
		/*Verify B Session Valid Bit to verify vbus status*/
		if (B_SESSION_VALID & n)	{
			pr_info("usb cable connected\n");
			ui->usb_state = USB_STATE_POWERED;
			ui->flags = USB_FLAG_VBUS_ONLINE;
			/* Wait for 100ms to stabilize VBUS before initializing
			 * USB and detecting charger type
			 */
			queue_delayed_work(usb_work, &ui->work, 0);
		} else {
			int i;

			usb_disable_pullup(ui);

			printk(KERN_INFO "usb cable disconnected\n");
			ui->usb_state = USB_STATE_NOTATTACHED;
			ui->configured = 0;
			for (i = 0; i < ui->num_funcs; i++) {
				struct usb_function_info *fi = ui->func[i];
				if (!fi ||
				!(ui->composition->functions & (1 << i)))
					continue;
				if (fi->func->disconnect)
					fi->func->disconnect
						(fi->func->context);
			}
			ui->flags = USB_FLAG_VBUS_OFFLINE;
			queue_delayed_work(usb_work, &ui->work, 0);
		}
	}

	return IRQ_HANDLED;
}

static void usb_prepare(struct usb_info *ui)
{
	memset(ui->buf, 0, 4096);
	ui->head = (void *) (ui->buf + 0);

	/* only important for reset/reinit */
	memset(ui->ept, 0, sizeof(ui->ept));
	ui->next_item = 0;
	ui->speed = USB_SPEED_UNKNOWN;

	init_endpoints(ui);

	ui->ep0in.max_pkt = 64;
	ui->ep0in.ui = ui;
	ui->ep0in.alloced = 1;
	ui->ep0out.max_pkt = 64;
	ui->ep0out.ui = ui;
	ui->ep0out.alloced = 1;

	ui->setup_req = usb_ept_alloc_req(&ui->ep0in, SETUP_BUF_SIZE);
	ui->ep0out_req = usb_ept_alloc_req(&ui->ep0out, ui->ep0out.max_pkt);

	INIT_WORK(&ui->chg_stop, usb_chg_stop);
	INIT_WORK(&ui->li.wakeup_phy, usb_lpm_wakeup_phy);
	INIT_DELAYED_WORK(&ui->work, usb_do_work);
	INIT_DELAYED_WORK(&ui->chg_legacy_det, usb_chg_legacy_detect);
}

static int usb_is_online(struct usb_info *ui)
{
	/* continue lpm if bus is suspended or disconnected or stopped*/
	if (((readl(USB_PORTSC) & PORTSC_SUSP) == PORTSC_SUSP) ||
			((readl(USB_PORTSC) & PORTSC_CCS) == 0) ||
			((readl(USB_USBCMD) & USBCMD_RS) == 0))
		return 0;

	pr_debug("usb is online\n");
	pr_debug("usbcmd:(%08x) usbsts:(%08x) portsc:(%08x)\n",
			readl(USB_USBCMD),
			readl(USB_USBSTS),
			readl(USB_PORTSC));
	return -1;
}

static int usb_wakeup_phy(struct usb_info *ui)
{
	int i;

	writel(readl(USB_USBCMD) & ~ULPI_STP_CTRL, USB_USBCMD);

	/* some circuits automatically clear PHCD bit */
	for (i = 0; i < 5 && (readl(USB_PORTSC) & PORTSC_PHCD); i++) {
		writel(readl(USB_PORTSC) & ~PORTSC_PHCD, USB_PORTSC);
		msleep(1);
	}

	if ((readl(USB_PORTSC) & PORTSC_PHCD)) {
		pr_err("%s: cannot clear phcd bit\n", __func__);
		return -1;
	}

	return 0;
}

static int usb_suspend_phy(struct usb_info *ui)
{
	int i;
	unsigned long flags;

	if (usb_is_online(ui))
		return -1;

	/* spec talks about following bits in LPM for external phy.
	 * But they are ignored because
	 * 1. disabling interface protection circuit: by disabling
	 * interface protection curcuit we cannot come out
	 * of lpm as async interrupts would be disabled
	 * 2. setting the suspendM bit: this bit would be set by usb
	 * controller once we set phcd bit.
	 */
	switch (PHY_TYPE(ui->phy_info)) {
	case USB_PHY_INTEGRATED:
		if (!is_phy_45nm())
			ulpi_read(ui, 0x14);

		/* turn on/off otg comparators */
		if (ui->vbus_sn_notif &&
			ui->usb_state == USB_STATE_NOTATTACHED)
			ulpi_write(ui, 0x00, 0x30);
		else
			ulpi_write(ui, 0x01, 0x30);

		if (!is_phy_45nm())
			ulpi_write(ui, 0x08, 0x09);

		break;

	case USB_PHY_UNDEFINED:
		pr_err("%s: undefined phy type\n", __func__);
		return -1;
	}

	/* loop for large amount of time */
	for (i = 0; i < 500; i++) {
		spin_lock_irqsave(&ui->lock, flags);
		if (usb_is_online(ui)) {
			spin_unlock_irqrestore(&ui->lock, flags);
			return -1;
		}
		/* set phy to be in lpm */
		writel(readl(USB_PORTSC) | PORTSC_PHCD, USB_PORTSC);
		spin_unlock_irqrestore(&ui->lock, flags);

		msleep(1);
		if (readl(USB_PORTSC) & PORTSC_PHCD)
			goto blk_stp_sig;
	}

	if (!(readl(USB_PORTSC) & PORTSC_PHCD)) {
		pr_err("unable to set phcd of portsc reg\n");
		pr_err("Reset HW link and phy to recover from phcd error\n");
		usb_hw_reset(ui);
		return -1;
	}

	/* we have to set this bit again to work-around h/w bug */
	writel(readl(USB_PORTSC) | PORTSC_PHCD, USB_PORTSC);

blk_stp_sig:
	/* block the stop signal */
	writel(readl(USB_USBCMD) | ULPI_STP_CTRL, USB_USBCMD);

	return 0;
}

/* SW workarounds
Issue#2		- Integrated PHY Calibration
Symptom		- Electrical compliance failure in eye-diagram tests
SW workaround		- Try to raise amplitude to 400mV

Issue#3		- AHB Posted Writes
Symptom		- USB stability
SW workaround		- This programs xtor ON, BURST disabled and
			unspecified length of INCR burst enabled
*/
static int usb_hw_reset(struct usb_info *ui)
{
	unsigned i;
	struct msm_hsusb_platform_data *pdata;
	unsigned long timeout;
	unsigned val = 0;

	pdata = ui->pdev->dev.platform_data;

	clk_enable(ui->clk);
	/* reset the phy before resetting link */
	if (readl(USB_PORTSC) & PORTSC_PHCD)
		usb_wakeup_phy(ui);
	/* rpc call for phy_reset */
	if (ui->pdata->phy_reset)
		ui->pdata->phy_reset(ui->addr);
	else
		msm_hsusb_phy_reset();
	/* Give some delay to settle phy after reset */
	msleep(100);

	/* RESET */
	writel(USBCMD_RESET, USB_USBCMD);
	timeout = jiffies + USB_LINK_RESET_TIMEOUT;
	while (readl(USB_USBCMD) & USBCMD_RESET) {
		if (time_after(jiffies, timeout)) {
			dev_err(&ui->pdev->dev, "usb link reset timeout\n");
			break;
		}
		msleep(1);
	}

	/* select DEVICE mode with SDIS active */
	writel((USBMODE_SDIS | USBMODE_DEVICE), USB_USBMODE);
	msleep(1);

	/* select ULPI phy */
	i = (readl(USB_PORTSC) & ~PORTSC_PTS);
	writel(i | PORTSC_PTS_ULPI, USB_PORTSC);
	/* set usb controller interrupt latency to zero*/
	writel((readl(USB_USBCMD) & ~USBCMD_ITC_MASK) | USBCMD_ITC(0),
							USB_USBCMD);

	/* If the target is 7x01 and roc version is > 1.2, set
	 * the AHB mode to 2 for maximum performance, else set
	 * it to 1, to bypass the AHB transactor for stability.
	 */
	if (PHY_TYPE(ui->phy_info) == USB_PHY_EXTERNAL) {
		if (pdata->soc_version >= SOC_ROC_2_0)
			writel(0x02, USB_ROC_AHB_MODE);
		else
			writel(0x01, USB_ROC_AHB_MODE);
	} else {
		unsigned cfg_val;

		/* Raise  amplitude to 400mV
		 * SW workaround, Issue#2
		 */
		cfg_val = ulpi_read(ui, ULPI_CONFIG_REG);
		cfg_val |= ULPI_AMPLITUDE_MAX;
		ulpi_write(ui, cfg_val, ULPI_CONFIG_REG);

		writel(0x0, USB_AHB_BURST);
		writel(0x00, USB_AHB_MODE);
	}

	/* TBD: do we have to add DpRise, ChargerRise and
	 * IdFloatRise for 45nm
	 */
	/* Disable VbusValid and SessionEnd comparators */
	val = ULPI_VBUS_VALID | ULPI_SESS_END;

	/* enable id interrupt only when transceiver is available */
	if (ui->xceiv)
		writel(readl(USB_OTGSC) | OTGSC_BSVIE | OTGSC_IDIE, USB_OTGSC);
	else {
		writel((readl(USB_OTGSC) | OTGSC_BSVIE) & ~OTGSC_IDPU,
							USB_OTGSC);
		ulpi_write(ui, ULPI_IDPU, ULPI_OTG_CTRL_CLR);
		val |= ULPI_HOST_DISCONNECT | ULPI_ID_GND;
	}
	ulpi_write(ui, val, ULPI_INT_RISE_CLR);
	ulpi_write(ui, val, ULPI_INT_FALL_CLR);

	/* we are just setting the pointer in the hwblock. Since the
	 * endpoint isnt enabled the hw block doenst read the contents
	 * of ui->dma - so we dont need a barrier here
	 * */
	writel(ui->dma, USB_ENDPOINTLISTADDR);

	clk_disable(ui->clk);

	return 0;
}

static void usb_reset(struct usb_info *ui)
{
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	ui->running = 0;
	spin_unlock_irqrestore(&ui->lock, flags);

#if 0
	/* we should flush and shutdown cleanly if already running */
	writel(0xffffffff, USB_ENDPTFLUSH);
	msleep(2);
#endif

	if (usb_hw_reset(ui)) {
		pr_info("%s: h/w reset failed\n", __func__);
		return;
	}

	usb_configure_endpoint(&ui->ep0in, NULL);
	usb_configure_endpoint(&ui->ep0out, NULL);

	/* marking us offline will cause ept queue attempts to fail */
	ui->online = 0;

	/* terminate any pending transactions */
	flush_all_endpoints(ui);

	set_configuration(ui, 0);

	spin_lock_irqsave(&ui->lock, flags);
	ui->running = 1;
	spin_unlock_irqrestore(&ui->lock, flags);
}

static void usb_enable(void *handle, int enable)
{
	struct usb_info *ui = handle;
	unsigned long flags;
	spin_lock_irqsave(&ui->lock, flags);

	if (enable) {
		ui->flags |= USB_FLAG_RESET;
		ui->active = 1;
		spin_unlock_irqrestore(&ui->lock, flags);
		usb_do_work(&ui->work.work);
	} else {
		ui->active = 0;
		spin_unlock_irqrestore(&ui->lock, flags);
		usb_clk_disable(ui);
		msm_hsusb_suspend_locks_acquire(ui, 0);
	}
}

static struct msm_otg_ops dcd_ops = {
	.request = usb_enable,
};

void usb_start(struct usb_info *ui)
{
	int i, ret;

	for (i = 0; i < ui->num_funcs; i++) {
		struct usb_function_info *fi = ui->func[i];
		if (!fi || !(ui->composition->functions & (1<<i)))
			continue;
		if (fi->enabled) {
			pr_info("usb_bind_func() (%s)\n", fi->func->name);
			fi->func->bind(fi->func->context);
		}
	}

	ui->clk_enabled = 0;
	ui->vreg_enabled = 0;

	ui->xceiv = msm_otg_get_transceiver();
	if (ui->xceiv) {
		ui->flags = USB_FLAG_REG_OTG;
		queue_delayed_work(usb_work, &ui->work, 0);
	} else {
		/*Initialize pm app RPC */
		ret = msm_pm_app_rpc_init();
		if (ret) {
			pr_err("%s: pm_app_rpc connect failed\n", __func__);
			goto out;
		}
		pr_info("%s: pm_app_rpc connect success\n", __func__);

		ret = msm_pm_app_register_vbus_sn(&msm_hsusb_set_vbus_state);
		if (ret) {
			pr_err("%s:PMIC VBUS SN notif not supported\n", \
					__func__);
			msm_pm_app_rpc_deinit();
			goto out;
		}
		pr_info("%s:PMIC VBUS SN notif supported\n", \
					__func__);

		ret = msm_pm_app_enable_usb_ldo(1);
		if (ret) {
			pr_err("%s: unable to turn on internal LDO", \
					__func__);
			msm_pm_app_unregister_vbus_sn(
					&msm_hsusb_set_vbus_state);
			msm_pm_app_rpc_deinit();
			goto out;
		}
		ui->vbus_sn_notif = 1;
out:
		ui->active = 1;
		ui->flags |= (USB_FLAG_START | USB_FLAG_RESET);
		queue_delayed_work(usb_work, &ui->work, 0);
	}

}

static LIST_HEAD(usb_function_list);
static DEFINE_MUTEX(usb_function_list_lock);


static struct usb_function_info *usb_find_function(const char *name)
{
	struct list_head *entry;
	list_for_each(entry, &usb_function_list) {
		struct usb_function_info *fi =
			list_entry(entry, struct usb_function_info, list);
		if (fi) {
			if (!strcmp(name, fi->func->name))
				return fi;
		}
	}

	return NULL;
}

static void usb_try_to_bind(void)
{
	struct usb_info *ui = the_usb_info;
	unsigned long enabled_functions = 0;
	int i;

	if (!ui || ui->bound || !ui->pdev || !ui->composition)
		return;

	for (i = 0; i < ui->num_funcs; i++) {
		if (ui->func[i])
			enabled_functions |= (1 << i);
	}
	if ((enabled_functions & ui->composition->functions)
					!= ui->composition->functions)
		return;

	usb_set_composition(ui->composition->product_id);
	usb_configure_device_descriptor(ui);

	/* we have found all the needed functions */
	ui->bound = 1;
	printk(KERN_INFO "msm_hsusb: functions bound. starting.\n");
	usb_start(ui);
}

static int usb_get_function_index(const char *name)
{
	struct usb_info *ui = the_usb_info;
	int i;

	for (i = 0; i < ui->num_funcs; i++) {
		if (!strcmp(name, ui->functions_map[i].name))
			return i;
	}
	return -1;
}

int usb_function_register(struct usb_function *driver)
{
	struct usb_info *ui = the_usb_info;
	struct usb_function_info *fi;
	int ret = 0;
	int index;

	mutex_lock(&usb_function_list_lock);

	index = usb_get_function_index(driver->name);
	if (index < 0) {
		pr_err("%s: unsupported function = %s\n",
				__func__, driver->name);
		ret = -EINVAL;
		goto fail;
	}

	fi = kzalloc(sizeof(*fi), GFP_KERNEL);
	if (!fi) {
		ret = -ENOMEM;
		goto fail;
	}

	fi->func = driver;
	list_add(&fi->list, &usb_function_list);
	ui->func[index] = fi;
	fi->func->ep0_out_req = ui->ep0out_req;
	fi->func->ep0_in_req = ui->setup_req;
	fi->func->ep0_out = &ui->ep0out;
	fi->func->ep0_in = &ui->ep0in;
	pr_info("%s: name = '%s',  map = %d\n", __func__, driver->name, index);

	usb_try_to_bind();
fail:
	mutex_unlock(&usb_function_list_lock);
	return ret;
}
EXPORT_SYMBOL(usb_function_register);

static unsigned short usb_validate_product_id(unsigned short pid)
{
	struct usb_info *ui = the_usb_info;
	int i;

	if (!ui || !ui->pdata)
		return -1;

	/* set idProduct based on which functions are enabled */
	for (i = 0; i < ui->pdata->num_compositions; i++) {
		if (ui->pdata->compositions[i].product_id == pid)
			break;
	}

	if (i < ui->pdata->num_compositions) {
		struct usb_composition *comp = &ui->pdata->compositions[i];
		for (i = 0; i < ui->num_funcs; i++) {
			if (comp->functions & (1 << i)) {
				if (!ui->func[i]) {
					pr_err("%s: func(%d) not available\n",
								__func__, i);
					return 0;
				}
			}
		}
		return comp->product_id;
	} else
		pr_err("%s: Product id (%x) is not supported\n", __func__, pid);
	return 0;
}

static unsigned short usb_get_product_id(unsigned long enabled_functions)
{
	struct usb_info *ui = the_usb_info;
	int i;

	if (!(ui && ui->pdata))
		return -1;

	/* set idProduct based on which functions are enabled */
	for (i = 0; i < ui->pdata->num_compositions; i++) {
		if (ui->pdata->compositions[i].functions == enabled_functions)
			return ui->pdata->compositions[i].product_id;
	}
	return 0;
}

static void usb_uninit(struct usb_info *ui)
{
	int i;

	for (i = 0; i < ui->strdesc_index; i++)
		kfree(ui->strdesc[i]);
	ui->strdesc_index = 1;
	ui->next_ifc_num = 0;
}

static unsigned short usb_set_composition(unsigned short pid)
{
	struct usb_info *ui = the_usb_info;
	int i;

	if (!(ui && ui->pdata))
		return 0;

	/* Retrieve product id on enabled functions */
	for (i = 0; i < ui->pdata->num_compositions; i++) {
		if (ui->pdata->compositions[i].product_id == pid) {
			ui->composition = &ui->pdata->compositions[i];
			for (i = 0; i < ui->num_funcs; i++) {
				struct usb_function_info *fi = ui->func[i];
				if (ui->func && fi && fi->func) {
					fi->enabled = (ui->composition->
							functions >> i) & 1;
				}
			}
			pr_info("%s: composition set to product id = %x\n",
				__func__, ui->composition->product_id);
			return ui->composition->product_id;
		}
	}
	pr_err("%s: product id (%x) not supported\n", __func__, pid);
	return 0;
}

static void usb_switch_composition(unsigned short pid)
{
	struct usb_info *ui = the_usb_info;
	int i;
	unsigned long flags;


	if (!ui->active)
		return;
	if (!usb_validate_product_id(pid))
		return;

	disable_irq(ui->irq);
	if (cancel_delayed_work_sync(&ui->work))
		pr_info("%s: Removed work successfully\n", __func__);
	if (ui->running) {
		spin_lock_irqsave(&ui->lock, flags);
		ui->running = 0;
		ui->online = 0;
		ui->bound = 0;
		spin_unlock_irqrestore(&ui->lock, flags);
		/* we should come out of lpm to access registers */
		if (ui->in_lpm) {
			if (PHY_TYPE(ui->phy_info) == USB_PHY_EXTERNAL) {
				disable_irq(ui->gpio_irq[0]);
				disable_irq(ui->gpio_irq[1]);
			}

			if (ui->usb_state == USB_STATE_NOTATTACHED
						&& ui->vbus_sn_notif)
				msm_pm_app_enable_usb_ldo(1);

			usb_lpm_exit(ui);
			if (cancel_work_sync(&ui->li.wakeup_phy))
				usb_lpm_wakeup_phy(NULL);
			ui->in_lpm = 0;
		}
		/* disable usb and session valid interrupts */
		writel(0, USB_USBINTR);
		writel(readl(USB_OTGSC) & ~OTGSC_BSVIE, USB_OTGSC);

		/* stop the controller */
		usb_disable_pullup(ui);
		ui->usb_state = USB_STATE_NOTATTACHED;
		switch_set_state(&ui->sdev, 0);
		/* Before starting again, wait for 300ms
		 * to make sure host detects soft disconnection
		 **/
		msleep(300);
	}

	for (i = 0; i < ui->num_funcs; i++) {
		struct usb_function_info *fi = ui->func[i];
		if (!fi || !fi->func || !fi->enabled)
			continue;
		if (fi->func->configure)
			fi->func->configure(0, fi->func->context);
		if (fi->func->unbind)
			fi->func->unbind(fi->func->context);
	}

	usb_uninit(ui);
	usb_set_composition(pid);
	usb_configure_device_descriptor(ui);

	/* initialize functions */
	for (i = 0; i < ui->num_funcs; i++) {
		struct usb_function_info *fi = ui->func[i];
		if (!fi || !(ui->composition->functions & (1 << i)))
			continue;
		if (fi->enabled) {
			if (fi->func->bind)
				fi->func->bind(fi->func->context);
		}
	}

	ui->bound = 1;
	ui->flags = USB_FLAG_RESET;
	queue_delayed_work(usb_work, &ui->work, 0);
	enable_irq(ui->irq);
}

void usb_function_enable(const char *function, int enable)
{
	struct usb_function_info *fi;
	struct usb_info *ui = the_usb_info;
	unsigned long functions_mask;
	int curr_enable;
	unsigned short pid;
	int i;

	if (!ui)
		return;

	pr_info("%s: name = %s, enable = %d\n", __func__, function, enable);

	fi = usb_find_function(function);
	if (!fi) {
		pr_err("%s: function (%s) not registered with DCD\n",
							__func__, function);
		return;
	}
	if (fi->enabled == enable) {
		pr_err("%s: function (%s) state is same\n",
						__func__, function);
		return;
	}
	functions_mask = 0;
	curr_enable = fi->enabled;
	fi->enabled = enable;
	for (i = 0; i < ui->num_funcs; i++) {
		struct usb_function_info *fi = ui->func[i];
		if (fi && fi->enabled)
			functions_mask |= (1 << i);
	}

	pid = usb_get_product_id(functions_mask);
	if (!pid) {
		fi->enabled = curr_enable;
		pr_err("%s: mask (%lx) not matching with any products\n",
						__func__, functions_mask);
		pr_err("%s: continuing with current composition\n", __func__);
		return;
	}
	usb_switch_composition(pid);
}
EXPORT_SYMBOL(usb_function_enable);

static int usb_free(struct usb_info *ui, int ret)
{
	disable_irq_wake(ui->irq);
	free_irq(ui->irq, ui);
	if (ui->gpio_irq[0])
		free_irq(ui->gpio_irq[0], NULL);
	if (ui->gpio_irq[1])
		free_irq(ui->gpio_irq[1], NULL);

	dma_pool_destroy(ui->pool);
	dma_free_coherent(&ui->pdev->dev, 4096, ui->buf, ui->dma);
	kfree(ui->func);
	kfree(ui->strdesc);
	iounmap(ui->addr);
	clk_put(ui->clk);
	clk_put(ui->pclk);
	clk_put(ui->cclk);
	msm_hsusb_suspend_locks_init(ui, 0);
	kfree(ui);

	return ret;
}

static int usb_vbus_is_on(struct usb_info *ui)
{
	unsigned tmp;

	/* disable session valid raising and falling interrupts */
	ulpi_write(ui, ULPI_SESSION_VALID_RAISE, ULPI_USBINTR_ENABLE_RASING_C);
	ulpi_write(ui, ULPI_SESSION_VALID_FALL, ULPI_USBINTR_ENABLE_FALLING_C);

	tmp = ulpi_read(ui, ULPI_USBINTR_STATUS);

	/* enable session valid raising and falling interrupts */
	ulpi_write(ui, ULPI_SESSION_VALID_RAISE, ULPI_USBINTR_ENABLE_RASING_S);
	ulpi_write(ui, ULPI_SESSION_VALID_FALL, ULPI_USBINTR_ENABLE_FALLING_S);

	if (tmp & (1 << 2))
		return 1;
	return 0;
}
static void usb_do_work(struct work_struct *w)
{
	struct usb_info *ui = container_of(w, struct usb_info, work.work);
	unsigned long iflags;
	unsigned long flags, ret;

	for (;;) {
		spin_lock_irqsave(&ui->lock, iflags);
		flags = ui->flags;
		ui->flags = 0;
		spin_unlock_irqrestore(&ui->lock, iflags);

		/* give up if we have nothing to do */
		if (flags == 0)
			break;

		switch (ui->state) {
		case USB_STATE_IDLE:
			if (flags & USB_FLAG_REG_OTG) {
				dcd_ops.handle = (void *) ui;
				ret = ui->xceiv->set_peripheral(ui->xceiv,
								&dcd_ops);
				if (ret)
					pr_err("%s: Can't register peripheral"
						"driver with OTG", __func__);
				break;
			}
			if ((flags & USB_FLAG_START) ||
					(flags & USB_FLAG_RESET)) {
				disable_irq(ui->irq);
				if (ui->vbus_sn_notif)
					msm_pm_app_enable_usb_ldo(1);
				usb_clk_enable(ui);
				usb_vreg_enable(ui);
				usb_vbus_online(ui);

				/* if VBUS is present move to ONLINE state
				 * otherwise move to OFFLINE state
				 */
				if (usb_vbus_is_on(ui)) {
					ui->usb_state = USB_STATE_POWERED;
					msm_hsusb_suspend_locks_acquire(ui, 1);
					ui->state = USB_STATE_ONLINE;
					usb_enable_pullup(ui);
					schedule_delayed_work(
							&ui->chg_legacy_det,
							USB_CHG_DET_DELAY);
					pr_info("hsusb: IDLE -> ONLINE\n");
				} else {
					ui->usb_state = USB_STATE_NOTATTACHED;
					ui->state = USB_STATE_OFFLINE;

					msleep(500);
					usb_lpm_enter(ui);
					pr_info("hsusb: IDLE -> OFFLINE\n");
					if (ui->vbus_sn_notif)
						msm_pm_app_enable_usb_ldo(0);
				}
				enable_irq(ui->irq);
				break;
			}
			goto reset;

		case USB_STATE_ONLINE:
			/* If at any point when we were online, we received
			 * the signal to go offline, we must honor it
			 */
			if (flags & USB_FLAG_VBUS_OFFLINE) {
				enum charger_type temp;
				unsigned long f;

				cancel_delayed_work_sync(&ui->chg_legacy_det);

				spin_lock_irqsave(&ui->lock, f);
				temp = ui->chg_type;
				ui->chg_type = USB_CHG_TYPE__INVALID;
				spin_unlock_irqrestore(&ui->lock, f);

				if (temp != USB_CHG_TYPE__INVALID) {
					/* re-acquire wakelock and restore axi
					 * freq if they have been reduced by
					 * charger work item
					 */
					msm_hsusb_suspend_locks_acquire(ui, 1);

					msm_chg_usb_i_is_not_available();
					msm_chg_usb_charger_disconnected();
				}

				/* reset usb core and usb phy */
				disable_irq(ui->irq);
				if (ui->in_lpm)
					usb_lpm_exit(ui);
				usb_vbus_offline(ui);
				usb_lpm_enter(ui);
				if ((ui->vbus_sn_notif) &&
				(ui->usb_state == USB_STATE_NOTATTACHED))
					msm_pm_app_enable_usb_ldo(0);
				ui->state = USB_STATE_OFFLINE;
				enable_irq(ui->irq);
				switch_set_state(&ui->sdev, 0);
				pr_info("hsusb: ONLINE -> OFFLINE\n");
				break;
			}
			if (flags & USB_FLAG_SUSPEND) {
				ui->usb_state = USB_STATE_SUSPENDED;
				usb_lpm_enter(ui);
				msm_hsusb_suspend_locks_acquire(ui, 1);
				break;
			}
			if ((flags & USB_FLAG_RESUME) ||
					(flags & USB_FLAG_CONFIGURE)) {
				int maxpower = usb_get_max_power(ui);

				if (maxpower > 0)
					msm_chg_usb_i_is_available(maxpower);

				if (flags & USB_FLAG_CONFIGURE)
					switch_set_state(&ui->sdev, 1);

				break;
			}
			goto reset;

		case USB_STATE_OFFLINE:
			/* If we were signaled to go online and vbus is still
			 * present when we received the signal, go online.
			 */
			if ((flags & USB_FLAG_VBUS_ONLINE)) {
				msm_hsusb_suspend_locks_acquire(ui, 1);
				disable_irq(ui->irq);
				ui->state = USB_STATE_ONLINE;
				if (ui->in_lpm)
					usb_lpm_exit(ui);
				usb_vbus_online(ui);
				if (!(B_SESSION_VALID & readl(USB_OTGSC))) {
					writel(((readl(USB_OTGSC) &
						~OTGSC_INTR_STS_MASK) |
						OTGSC_BSVIS), USB_OTGSC);
					enable_irq(ui->irq);
					goto reset;
				}
				usb_enable_pullup(ui);
				schedule_delayed_work(
						&ui->chg_legacy_det,
						USB_CHG_DET_DELAY);
				pr_info("hsusb: OFFLINE -> ONLINE\n");
				enable_irq(ui->irq);
				break;
			}
			if (flags & USB_FLAG_SUSPEND) {
				usb_lpm_enter(ui);
				wake_unlock(&ui->wlock);
				break;
			}
		default:
reset:
			/* For RESET or any unknown flag in a particular state
			 * go to IDLE state and reset HW to bring to known state
			 */
			ui->flags = USB_FLAG_RESET;
			ui->state = USB_STATE_IDLE;
		}
	}
}

void msm_hsusb_set_vbus_state(int online)
{
	struct usb_info *ui = the_usb_info;

	if (ui && online) {
		msm_pm_app_enable_usb_ldo(1);
		usb_lpm_exit(ui);
		/* Turn on PHY comparators */
		if (!(ulpi_read(ui, 0x30) & 0x01))
				ulpi_write(ui, 0x01, 0x30);
	}
}

static irqreturn_t usb_lpm_gpio_isr(int irq, void *data)
{
	disable_irq(irq);

	return IRQ_HANDLED;
}

static void usb_lpm_exit(struct usb_info *ui)
{
	if (ui->in_lpm == 0)
		return;

	if (usb_lpm_config_gpio)
		usb_lpm_config_gpio(0);

	wake_lock(&ui->wlock);
	usb_clk_enable(ui);
	usb_vreg_enable(ui);

	writel(readl(USB_USBCMD) & ~ASYNC_INTR_CTRL, USB_USBCMD);
	writel(readl(USB_USBCMD) & ~ULPI_STP_CTRL, USB_USBCMD);

	if (readl(USB_PORTSC) & PORTSC_PHCD) {
		disable_irq(ui->irq);
		schedule_work(&ui->li.wakeup_phy);
	} else {
		ui->in_lpm = 0;
		if (ui->xceiv)
			ui->xceiv->set_suspend(ui->xceiv, 0);
	}
	pr_info("%s(): USB exited from low power mode\n", __func__);
}

static int usb_lpm_enter(struct usb_info *ui)
{
	unsigned long flags;
	unsigned connected;

	spin_lock_irqsave(&ui->lock, flags);
	if (ui->in_lpm) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_debug("already in lpm, nothing to do\n");
		return 0;
	}

	if (usb_is_online(ui)) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_info("%s: lpm procedure aborted\n", __func__);
		return -1;
	}

	ui->in_lpm = 1;
	if (ui->xceiv)
		ui->xceiv->set_suspend(ui->xceiv, 1);
	disable_irq(ui->irq);
	spin_unlock_irqrestore(&ui->lock, flags);

	if (usb_suspend_phy(ui)) {
		ui->in_lpm = 0;
		ui->flags = USB_FLAG_RESET;
		enable_irq(ui->irq);
		pr_err("%s: phy suspend failed, lpm procedure aborted\n",
				__func__);
		return -1;
	}

	if ((B_SESSION_VALID & readl(USB_OTGSC)) &&
				(ui->usb_state == USB_STATE_NOTATTACHED)) {
		ui->in_lpm = 0;
		writel(((readl(USB_OTGSC) & ~OTGSC_INTR_STS_MASK) |
						OTGSC_BSVIS), USB_OTGSC);
		ui->flags = USB_FLAG_VBUS_ONLINE;
		ui->usb_state = USB_STATE_POWERED;
		usb_wakeup_phy(ui);
		enable_irq(ui->irq);
		return -1;
	}

	/* enable async interrupt */
	writel(readl(USB_USBCMD) | ASYNC_INTR_CTRL, USB_USBCMD);
	connected = readl(USB_USBCMD) & USBCMD_RS;

	usb_vreg_disable(ui);
	usb_clk_disable(ui);

	if (usb_lpm_config_gpio) {
		if (usb_lpm_config_gpio(1)) {
			spin_lock_irqsave(&ui->lock, flags);
			usb_lpm_exit(ui);
			spin_unlock_irqrestore(&ui->lock, flags);
			enable_irq(ui->irq);
			return -1;
		}
		enable_irq(ui->gpio_irq[0]);
		enable_irq(ui->gpio_irq[1]);
	}

	enable_irq(ui->irq);
	msm_hsusb_suspend_locks_acquire(ui, 0);
	pr_info("%s: usb in low power mode\n", __func__);
	return 0;
}

static void usb_enable_pullup(struct usb_info *ui)
{
	disable_irq(ui->irq);
	writel(STS_URI | STS_SLI | STS_UI | STS_PCI, USB_USBINTR);
	writel(readl(USB_USBCMD) | USBCMD_RS, USB_USBCMD);
	enable_irq(ui->irq);
}

/* SW workarounds
Issue #1	- USB Spoof Disconnect Failure
Symptom	- Writing 0 to run/stop bit of USBCMD doesn't cause disconnect
SW workaround	- Making opmode non-driving and SuspendM set in function
		register of SMSC phy
*/
static void usb_disable_pullup(struct usb_info *ui)
{
	disable_irq(ui->irq);
	writel(readl(USB_USBINTR) & ~(STS_URI | STS_SLI | STS_UI | STS_PCI),
			USB_USBINTR);
	writel(readl(USB_USBCMD) & ~USBCMD_RS, USB_USBCMD);

	/* S/W workaround, Issue#1 */
	if (!is_phy_external() && !is_phy_45nm())
		ulpi_write(ui, 0x48, 0x04);

	enable_irq(ui->irq);
}

static void usb_chg_stop(struct work_struct *w)
{
	struct usb_info *ui = the_usb_info;
	enum charger_type temp;
	unsigned long flags;

	spin_lock_irqsave(&ui->lock, flags);
	temp = ui->chg_type;
	spin_unlock_irqrestore(&ui->lock, flags);

	if (temp == USB_CHG_TYPE__SDP)
		msm_chg_usb_i_is_not_available();
}

static void usb_vbus_online(struct usb_info *ui)
{
	if (ui->in_lpm) {
		if (usb_lpm_config_gpio)
			usb_lpm_config_gpio(0);
		usb_vreg_enable(ui);
		usb_clk_enable(ui);
		usb_wakeup_phy(ui);
		ui->in_lpm = 0;
	}

	usb_reset(ui);
}

static void usb_vbus_offline(struct usb_info *ui)
{
	unsigned long timeout;
	unsigned val = 0;

	if (ui->online != 0) {
		ui->online = 0;
		flush_all_endpoints(ui);
		set_configuration(ui, 0);
	}

	/* reset h/w at cable disconnetion becasuse
	 * of h/w bugs and to flush any resource that
	 * h/w might be holding
	 */
	clk_enable(ui->clk);

	if (readl(USB_PORTSC) & PORTSC_PHCD)
		usb_wakeup_phy(ui);

	if (ui->pdata->phy_reset)
		ui->pdata->phy_reset(ui->addr);
	else
		msm_hsusb_phy_reset();
	/* Give some delay to settle phy after reset */
	msleep(100);

	writel(USBCMD_RESET, USB_USBCMD);
	timeout = jiffies + USB_LINK_RESET_TIMEOUT;
	while (readl(USB_USBCMD) & USBCMD_RESET) {
		if (time_after(jiffies, timeout)) {
			dev_err(&ui->pdev->dev, "usb link reset timeout\n");
			break;
		}
		msleep(1);
	}

	/* Disable VbusValid and SessionEnd comparators */
	val = ULPI_VBUS_VALID | ULPI_SESS_END;

	/* enable id interrupt only when transceiver is available */
	if (ui->xceiv)
		writel(readl(USB_OTGSC) | OTGSC_BSVIE | OTGSC_IDIE, USB_OTGSC);
	else {
		writel((readl(USB_OTGSC) | OTGSC_BSVIE) & ~OTGSC_IDPU,
							USB_OTGSC);
		ulpi_write(ui, ULPI_IDPU, ULPI_OTG_CTRL_CLR);
		val |= ULPI_HOST_DISCONNECT | ULPI_ID_GND;
	}
	ulpi_write(ui, val, ULPI_INT_RISE_CLR);
	ulpi_write(ui, val, ULPI_INT_FALL_CLR);

	clk_disable(ui->clk);
}

static void usb_lpm_wakeup_phy(struct work_struct *w)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;

	if (usb_wakeup_phy(ui)) {
		pr_err("fatal error: cannot bring phy out of lpm\n");
		pr_err("%s: resetting controller\n", __func__);

		spin_lock_irqsave(&ui->lock, flags);
		usb_disable_pullup(ui);
		ui->flags = USB_FLAG_RESET;
		queue_delayed_work(usb_work, &ui->work, 0);
		enable_irq(ui->irq);
		spin_unlock_irqrestore(&ui->lock, flags);
		return;
	}

	ui->in_lpm = 0;
	if (ui->xceiv)
		ui->xceiv->set_suspend(ui->xceiv, 0);
	enable_irq(ui->irq);
}

void usb_function_reenumerate(void)
{
	struct usb_info *ui = the_usb_info;

	/* disable and re-enable the D+ pullup */
	pr_info("hsusb: disable pullup\n");
	usb_disable_pullup(ui);

	msleep(10);

	pr_info("hsusb: enable pullup\n");
	usb_enable_pullup(ui);
}

#if defined(CONFIG_DEBUG_FS)
static char debug_buffer[PAGE_SIZE];

static ssize_t debug_read_status(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct usb_info *ui = file->private_data;
	char *buf = debug_buffer;
	unsigned long flags;
	struct usb_endpoint *ept;
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
		if (ept->max_pkt == 0)
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
				       req->live ? 'L' : ' '
				);
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
	queue_delayed_work(usb_work, &ui->work, 0);
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

static struct dentry *debugfs_dent;
static struct dentry *debugfs_status;
static struct dentry *debugfs_reset;
static struct dentry *debugfs_cycle;
static void usb_debugfs_init(struct usb_info *ui)
{
	debugfs_dent = debugfs_create_dir("usb", 0);
	if (IS_ERR(debugfs_dent))
		return;

	debugfs_status = debugfs_create_file("status", 0444,
				debugfs_dent, ui, &debug_stat_ops);
	debugfs_reset = debugfs_create_file("reset", 0222,
				debugfs_dent, ui, &debug_reset_ops);
	debugfs_cycle = debugfs_create_file("cycle", 0222,
				debugfs_dent, ui, &debug_cycle_ops);
}

static void usb_debugfs_uninit(void)
{
	debugfs_remove(debugfs_status);
	debugfs_remove(debugfs_reset);
	debugfs_remove(debugfs_cycle);
	debugfs_remove(debugfs_dent);
}

#else
static void usb_debugfs_init(struct usb_info *ui) {}
static void usb_debugfs_uninit(void) {}
#endif

static void usb_configure_device_descriptor(struct usb_info *ui)
{
	desc_device.idVendor = ui->pdata->vendor_id;
	desc_device.idProduct = ui->composition->product_id;
	desc_device.bcdDevice = ui->pdata->version;

	if (ui->pdata->serial_number)
		desc_device.iSerialNumber =
			usb_msm_get_next_strdesc_id(ui->pdata->serial_number);
	if (ui->pdata->product_name)
		desc_device.iProduct =
			usb_msm_get_next_strdesc_id(ui->pdata->product_name);
	if (ui->pdata->manufacturer_name)
		desc_device.iManufacturer =
			usb_msm_get_next_strdesc_id(
				ui->pdata->manufacturer_name);

	/* Send Serial number to A9 for software download */
	if (ui->pdata->serial_number) {
		msm_hsusb_is_serial_num_null(FALSE);
		msm_hsusb_send_serial_number(ui->pdata->serial_number);
	} else
		msm_hsusb_is_serial_num_null(TRUE);

	msm_hsusb_send_productID(desc_device.idProduct);

}
static ssize_t msm_hsusb_store_func_enable(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	char name[20];
	int enable = 0;
	int i;

	for (i = 0; buf[i] != 0; i++) {
		if (buf[i] == '=')
			break;
		name[i] = buf[i];
	}
	name[i++] = 0;
	if (buf[i] == '0' || buf[i] == '1')
		enable = buf[i] - '0';
	else
		return size;

	pr_info("%s: name = %s, enable = %d\n", __func__, name, enable);
	usb_function_enable(name, enable);
	return size;
}
static ssize_t msm_hsusb_show_compswitch(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct usb_info *ui = the_usb_info;
	int i;

	if (ui->composition)
		i = scnprintf(buf, PAGE_SIZE,
				"composition product id = %x\n",
					ui->composition->product_id);
	else
		i = scnprintf(buf, PAGE_SIZE,
				"composition product id = 0\n");
	return i;
}

static ssize_t msm_hsusb_store_compswitch(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	unsigned long pid;

	if (!strict_strtoul(buf, 16, &pid)) {
		pr_info("%s: Requested New Product id = %lx\n", __func__, pid);
		usb_switch_composition((unsigned short)pid);
	} else
		pr_info("%s: strict_strtoul conversion failed\n", __func__);

	return size;
}
static ssize_t msm_hsusb_store_autoresume(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	usb_remote_wakeup();

	return size;
}

static ssize_t msm_hsusb_show_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct usb_info *ui = the_usb_info;
	int i;
	char *state[] = {"USB_STATE_NOTATTACHED", "USB_STATE_ATTACHED",
			"USB_STATE_POWERED", "USB_STATE_UNAUTHENTICATED",
			"USB_STATE_RECONNECTING", "USB_STATE_DEFAULT",
			"USB_STATE_ADDRESS", "USB_STATE_CONFIGURED",
			"USB_STATE_SUSPENDED"
	};

	i = scnprintf(buf, PAGE_SIZE, "%s\n", state[ui->usb_state]);
	return i;
}

static ssize_t msm_hsusb_show_lpm(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct usb_info *ui = the_usb_info;
	int i;

	i = scnprintf(buf, PAGE_SIZE, "%d\n", ui->in_lpm);
	return i;
}

static ssize_t msm_hsusb_show_speed(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct usb_info *ui = the_usb_info;
	int i;
	char *speed[] = {"USB_SPEED_UNKNOWN", "USB_SPEED_LOW",
			"USB_SPEED_FULL", "USB_SPEED_HIGH"};

	i = scnprintf(buf, PAGE_SIZE, "%s\n", speed[ui->speed]);
	return i;
}

static DEVICE_ATTR(composition, 0664,
		msm_hsusb_show_compswitch, msm_hsusb_store_compswitch);
static DEVICE_ATTR(func_enable, S_IWUSR,
		NULL, msm_hsusb_store_func_enable);
static DEVICE_ATTR(autoresume, 0222,
		NULL, msm_hsusb_store_autoresume);
static DEVICE_ATTR(state, 0664, msm_hsusb_show_state, NULL);
static DEVICE_ATTR(lpm, 0664, msm_hsusb_show_lpm, NULL);
static DEVICE_ATTR(speed, 0664, msm_hsusb_show_speed, NULL);

static struct attribute *msm_hsusb_attrs[] = {
	&dev_attr_composition.attr,
	&dev_attr_func_enable.attr,
	&dev_attr_autoresume.attr,
	&dev_attr_state.attr,
	&dev_attr_lpm.attr,
	&dev_attr_speed.attr,
	NULL,
};
static struct attribute_group msm_hsusb_attr_grp = {
	.attrs = msm_hsusb_attrs,
};

#define msm_hsusb_func_attr(function, index)				\
static ssize_t  show_##function(struct device *dev,			\
		struct device_attribute *attr, char *buf)		\
{									\
	struct usb_info *ui = the_usb_info;				\
	struct usb_function_info *fi = ui->func[index];			\
									\
	return sprintf(buf, "%d", fi->enabled);				\
									\
}									\
									\
static DEVICE_ATTR(function, S_IRUGO, show_##function, NULL);

msm_hsusb_func_attr(diag, 0);
msm_hsusb_func_attr(adb, 1);
msm_hsusb_func_attr(modem, 2);
msm_hsusb_func_attr(nmea, 3);
msm_hsusb_func_attr(mass_storage, 4);
msm_hsusb_func_attr(ethernet, 5);
msm_hsusb_func_attr(rmnet, 6);

static struct attribute *msm_hsusb_func_attrs[] = {
	&dev_attr_diag.attr,
	&dev_attr_adb.attr,
	&dev_attr_modem.attr,
	&dev_attr_nmea.attr,
	&dev_attr_mass_storage.attr,
	&dev_attr_ethernet.attr,
	&dev_attr_rmnet.attr,
	NULL,
};

static struct attribute_group msm_hsusb_func_attr_grp = {
	.name  = "functions",
	.attrs = msm_hsusb_func_attrs,
};

static int __init usb_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct usb_info *ui;
	int irq;
	int ulpi_irq1 = 0;
	int ulpi_irq2 = 0;
	int i;
	int ret = 0;

	if (!pdev || !pdev->dev.platform_data) {
		pr_err("%s:pdev or platform data is null\n", __func__);
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("%s: failed to get irq num from platform_get_irq\n",
				__func__);
		return -ENODEV;
	}


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s: failed to get mem resource\n", __func__);
		return -ENODEV;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &msm_hsusb_attr_grp);
	if (ret) {
		pr_err("%s: unable to create sysfs group\n", __func__);
		return ret;
	}

	usb_work = create_singlethread_workqueue("usb_work");
	if (!usb_work) {
		pr_err("%s: unable to create work queue\n", __func__);
		ret = -ENOMEM;
		goto free_sysfs_grp;
	}

	ui = kzalloc(sizeof(struct usb_info), GFP_KERNEL);
	if (!ui) {
		pr_err("%s: unable to allocate memory for ui\n", __func__);
		ret = -ENOMEM;
		goto free_workqueue;
	}

	ui->pdev = pdev;
	ui->pdata = pdev->dev.platform_data;

	for (i = 0; i < ui->pdata->num_compositions; i++)
		if (ui->pdata->compositions[i].product_id == pid) {
			ui->composition = &ui->pdata->compositions[i];
			break;
		}
	if (!ui->composition) {
		pr_err("%s: unable to find the composition with pid:(%d)\n",
				__func__, pid);
		ret = -ENODEV;
		goto free_ui;
	}

	ui->phy_info = ui->pdata->phy_info;
	if (ui->phy_info == USB_PHY_UNDEFINED) {
		pr_err("undefined phy_info: (%d)\n", ui->phy_info);
		ret = -ENOMEM;
		goto free_ui;
	}

	/* zero is reserved for language id */
	ui->strdesc_index = 1;
	ui->strdesc = kzalloc(sizeof(char *) * MAX_STRDESC_NUM, GFP_KERNEL);
	if (!ui->strdesc) {
		pr_err("%s: unable allocate mem for string descriptors\n",
				__func__);
		ret = -ENOMEM;
		goto free_ui;
	}

	ui->num_funcs = ui->pdata->num_functions;
	ui->func = kzalloc(sizeof(struct usb_function *) * ui->num_funcs,
				GFP_KERNEL);
	if (!ui->func) {
		pr_err("%s: unable allocate mem for functions\n", __func__);
		ret = -ENOMEM;
		goto free_str_desc;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &msm_hsusb_func_attr_grp);
	if (ret) {
		pr_err("%s: unable to create functions sysfs group\n",
				__func__);
		goto free_func;
	}

	ui->addr = ioremap(res->start, resource_size(res));
	if (!ui->addr) {
		pr_err("%s: unable ioremap\n", __func__);
		ret = -ENOMEM;
		goto free_func_sysfs_grp;
	}

	ui->buf = dma_alloc_coherent(&pdev->dev, 4096, &ui->dma, GFP_KERNEL);
	if (!ui->buf) {
		pr_err("%s: failed allocate dma coherent memory\n", __func__);
		ret = -ENOMEM;
		goto free_iounmap;
	}

	ui->pool = dma_pool_create("hsusb", NULL, 32, 32, 0);
	if (!ui->pool) {
		pr_err("%s: unable to allocate dma pool\n", __func__);
		ret = -ENOMEM;
		goto free_dma_coherent;
	}

	ui->clk = clk_get(&pdev->dev, "usb_hs_clk");
	if (IS_ERR(ui->clk)) {
		pr_err("%s: unable get usb_hs_clk\n", __func__);
		ret = PTR_ERR(ui->clk);
		goto free_dma_pool;
	}

	ui->pclk = clk_get(&pdev->dev, "usb_hs_pclk");
	if (IS_ERR(ui->pclk)) {
		pr_err("%s: unable get usb_hs_pclk\n", __func__);
		ret = PTR_ERR(ui->pclk);
		goto free_hs_clk;
	}

	if (ui->pdata->core_clk) {
		ui->cclk = clk_get(&pdev->dev, "usb_hs_core_clk");
		if (IS_ERR(ui->cclk)) {
			pr_err("%s: unable get usb_hs_core_clk\n", __func__);
			ret = PTR_ERR(ui->cclk);
			goto free_hs_pclk;
		}
	}

	if (ui->pdata->vreg5v_required) {
		ui->vreg = vreg_get(NULL, "boost");
		if (IS_ERR(ui->vreg)) {
			pr_err("%s: vreg get failed\n", __func__);
			ui->vreg = NULL;
			ret = PTR_ERR(ui->vreg);
			goto free_hs_cclk;
		}
	}

	/* disable interrupts before requesting irq */
	usb_clk_enable(ui);
	writel(0, USB_USBINTR);
	writel(readl(USB_OTGSC) & ~OTGSC_INTR_MASK, USB_OTGSC);
	usb_clk_disable(ui);

	ret = request_irq(irq, usb_interrupt, IRQF_SHARED, pdev->name, ui);
	if (ret) {
		pr_err("%s: request_irq failed\n", __func__);
		goto free_vreg5v;
	}
	ui->irq = irq;

	if (ui->pdata->config_gpio) {
		usb_lpm_config_gpio = ui->pdata->config_gpio;

		ulpi_irq1 = platform_get_irq_byname(pdev, "vbus_interrupt");
		if (ulpi_irq1 < 0) {
			pr_err("%s: failed to get vbus gpio interrupt\n",
					__func__);
			return -ENODEV;
		}

		ulpi_irq2 = platform_get_irq_byname(pdev, "id_interrupt");
		if (ulpi_irq2 < 0) {
			pr_err("%s: failed to get id gpio interrupt\n",
					__func__);
			return -ENODEV;
		}

		ret = request_irq(ulpi_irq1,
				&usb_lpm_gpio_isr,
				IRQF_TRIGGER_HIGH,
				"vbus_interrupt", NULL);
		if (ret) {
			pr_err("%s: failed to request vbus interrupt:(%d)\n",
					__func__, ulpi_irq1);
			goto free_irq;
		}

		ret = request_irq(ulpi_irq2,
				&usb_lpm_gpio_isr,
				IRQF_TRIGGER_RISING,
				"usb_ulpi_data3", NULL);
		if (ret) {
			pr_err("%s: failed to request irq ulpi_data_3:(%d)\n",
							__func__, ulpi_irq2);
			goto free_ulpi_irq1;
		}

		ui->gpio_irq[0] = ulpi_irq1;
		ui->gpio_irq[1] = ulpi_irq2;
	}

	ui->sdev.name = DRIVER_NAME;
	ui->sdev.print_name = print_switch_name;
	ui->sdev.print_state = print_switch_state;

	ret = switch_dev_register(&ui->sdev);
	if (ret < 0) {
		pr_err("%s(): switch_dev_register failed ret = %d\n",
				__func__, ret);
		goto free_ulpi_irq2;
	}

	the_usb_info = ui;
	ui->functions_map = ui->pdata->function_map;
	ui->selfpowered = 0;
	ui->remote_wakeup = 0;
	ui->maxpower = 0xFA;
	ui->chg_type = USB_CHG_TYPE__INVALID;
	/* to allow swfi latency, driver latency
	 * must be above listed swfi latency
	 */
	ui->pdata->swfi_latency += 1;

	spin_lock_init(&ui->lock);
	msm_hsusb_suspend_locks_init(ui, 1);
	enable_irq_wake(irq);

	/* memory barrier initialization in non-interrupt context */
	dmb();

	usb_debugfs_init(ui);
	usb_prepare(ui);

	pr_info("%s: io=%p, irq=%d, dma=%p(%x)\n",
			__func__, ui->addr, ui->irq, ui->buf, ui->dma);
	return 0;

free_ulpi_irq2:
	free_irq(ulpi_irq2, NULL);
free_ulpi_irq1:
	free_irq(ulpi_irq1, NULL);
free_irq:
	free_irq(ui->irq, ui);
free_vreg5v:
	if (ui->pdata->vreg5v_required)
		vreg_put(ui->vreg);
free_hs_cclk:
	clk_put(ui->cclk);
free_hs_pclk:
	clk_put(ui->pclk);
free_hs_clk:
	clk_put(ui->clk);
free_dma_pool:
	dma_pool_destroy(ui->pool);
free_dma_coherent:
	dma_free_coherent(&pdev->dev, 4096, ui->buf, ui->dma);
free_iounmap:
	iounmap(ui->addr);
free_func_sysfs_grp:
	sysfs_remove_group(&pdev->dev.kobj, &msm_hsusb_func_attr_grp);
free_func:
	kfree(ui->func);
free_str_desc:
	kfree(ui->strdesc);
free_ui:
	kfree(ui);
free_workqueue:
	destroy_workqueue(usb_work);
free_sysfs_grp:
	sysfs_remove_group(&pdev->dev.kobj, &msm_hsusb_attr_grp);

	return ret;
}

#ifdef CONFIG_PM
static int usb_platform_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ui->lock, flags);

	if (!ui->active) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_info("%s: peripheral mode is not active"
				"nothing to be done\n", __func__);
		return 0;
	}

	if (ui->in_lpm) {
		spin_unlock_irqrestore(&ui->lock, flags);
		pr_info("%s: we are already in lpm, nothing to be done\n",
					__func__);
		return 0;
	}
	spin_unlock_irqrestore(&ui->lock, flags);

	ret = usb_lpm_enter(ui);
	if (ret)
		pr_err("%s: failed to enter lpm\n", __func__);

	return ret;
}
#endif

static struct platform_driver usb_driver = {
	.probe = usb_probe,
#ifdef CONFIG_PM
	.suspend = usb_platform_suspend,
#endif
	.driver = { .name = DRIVER_NAME, },
};

static int __init usb_module_init(void)
{
	/* rpc connect for phy_reset */
	msm_hsusb_rpc_connect();
	/* rpc connect for charging */
	msm_chg_rpc_connect();

	return platform_driver_register(&usb_driver);
}

static void free_usb_info(void)
{
	struct usb_info *ui = the_usb_info;
	unsigned long flags;
	int i;
	if (ui) {
		INIT_LIST_HEAD(&usb_function_list);

		for (i = 0; i < ui->num_funcs; i++)
			kfree(ui->func[i]);
		ui->num_funcs = 0;
		usb_uninit(ui);
		kfree(ui->strdesc);
		usb_ept_free_req(&ui->ep0in, ui->setup_req);
		if (ui->ept[0].ui == ui)
			flush_all_endpoints(ui);
		spin_lock_irqsave(&ui->lock, flags);
		usb_clk_disable(ui);
		usb_vreg_disable(ui);
		spin_unlock_irqrestore(&ui->lock, flags);
		usb_free(ui, 0);
		the_usb_info = NULL;
	}
}
static void usb_exit(void)
{
	struct usb_info *ui = the_usb_info;
	/* free the dev state structure */
	if (!ui)
		return;

	if (ui->xceiv) {
		ui->xceiv->set_peripheral(ui->xceiv, NULL);
		msm_otg_put_transceiver(ui->xceiv);
	}

	cancel_work_sync(&ui->li.wakeup_phy);

	destroy_workqueue(usb_work);
	/* free the usb_info structure */
	free_usb_info();
	switch_dev_unregister(&ui->sdev);
	sysfs_remove_group(&ui->pdev->dev.kobj, &msm_hsusb_func_attr_grp);
	sysfs_remove_group(&ui->pdev->dev.kobj, &msm_hsusb_attr_grp);
	usb_debugfs_uninit();
	platform_driver_unregister(&usb_driver);
	msm_hsusb_rpc_close();
	msm_chg_rpc_close();
	msm_pm_app_unregister_vbus_sn(&msm_hsusb_set_vbus_state);
	msm_pm_app_rpc_deinit();
}

static void __exit usb_module_exit(void)
{
	usb_exit();
}

module_param(pid, int, 0);
MODULE_PARM_DESC(pid, "Product ID of the desired composition");

module_init(usb_module_init);
module_exit(usb_module_exit);

static void copy_string_descriptor(char *string, char *buffer)
{
	int length, i;

	if (string) {
		length = strlen(string);
		buffer[0] = 2 * length + 2;
		buffer[1] = USB_DT_STRING;
		for (i = 0; i < length; i++) {
			buffer[2 * i + 2] = string[i];
			buffer[2 * i + 3] = 0;
		}
	}
}
static int get_qualifier_descriptor(struct usb_qualifier_descriptor *dq)
{
	struct usb_qualifier_descriptor *dev_qualifier = dq;
	dev_qualifier->bLength = sizeof(struct usb_qualifier_descriptor),
	dev_qualifier->bDescriptorType = USB_DT_DEVICE_QUALIFIER,
	dev_qualifier->bcdUSB =  __constant_cpu_to_le16(0x0200),
	dev_qualifier->bDeviceClass = USB_CLASS_PER_INTERFACE,
	dev_qualifier->bDeviceSubClass = 0;
	dev_qualifier->bDeviceProtocol = 0;
	dev_qualifier->bMaxPacketSize0 = 64;
	dev_qualifier->bNumConfigurations = 1;
	dev_qualifier->bRESERVED = 0;
	return sizeof(struct usb_qualifier_descriptor);
}

static int usb_fill_descriptors(void *ptr,
		struct usb_descriptor_header **descriptors)
{
	unsigned char *buf = ptr;
	struct usb_descriptor_header *item = descriptors[0];
	unsigned cnt = 0;

	while (NULL != item) {
		unsigned len = item->bLength;
		memcpy(buf, item, len);
		buf += len;
		cnt++;
		item = descriptors[cnt];
	}

	return buf-(u8 *)ptr;
}

static int usb_find_descriptor(struct usb_info *ui, struct usb_ctrlrequest *ctl,
				struct usb_request *req)
{
	int i;
	unsigned short id = ctl->wValue;
	unsigned short type = id >> 8;
	id &= 0xff;

	if ((type == USB_DT_DEVICE) && (id == 0)) {
		req->length = sizeof(desc_device);
		if (usb_msm_is_iad()) {
			desc_device.bDeviceClass = 0xEF;
			desc_device.bDeviceSubClass = 0x02;
			desc_device.bDeviceProtocol = 0x01;
		}
		memcpy(req->buf, &desc_device, req->length);
		return 0;
	}
	if ((type == USB_DT_DEVICE_QUALIFIER) && (id == 0)) {
		struct usb_qualifier_descriptor dq;
		req->length = get_qualifier_descriptor(&dq);
		if (usb_msm_is_iad()) {
			dq.bDeviceClass = 0xEF;
			dq.bDeviceSubClass = 0x02;
			dq.bDeviceProtocol = 0x01;
		}
		memcpy(req->buf, &dq, req->length);
		return 0;
	}

	if ((type == USB_DT_OTHER_SPEED_CONFIG) && (id == 0))
		goto get_config;

	if ((type == USB_DT_CONFIG) && (id == 0)) {
		struct usb_config_descriptor cfg;
		unsigned ifc_count = 0;
		char *ptr, *start;
get_config:
		ifc_count = 0;
		start = req->buf;
		ptr = start + USB_DT_CONFIG_SIZE;
		ifc_count = ui->next_ifc_num;

		for (i = 0; i < ui->num_funcs; i++) {
			struct usb_function_info *fi = ui->func[i];
			struct usb_descriptor_header **dh = NULL;

			if (!fi || !(ui->composition->functions & (1 << i)))
				continue;
			switch (ui->speed) {
			case USB_SPEED_HIGH:
				if (type == USB_DT_OTHER_SPEED_CONFIG)
					dh = fi->func->fs_descriptors;
				else
					dh = fi->func->hs_descriptors;
				break;

			case USB_SPEED_FULL:
				if (type == USB_DT_OTHER_SPEED_CONFIG)
					dh = fi->func->hs_descriptors;
				else
					dh = fi->func->fs_descriptors;
				break;

			default:
				printk(KERN_ERR "Unsupported speed(%x)\n",
						ui->speed);
				return -1;
			}
			ptr += usb_fill_descriptors(ptr, dh);
		}

#define	USB_REMOTE_WAKEUP_SUPPORT	1
		cfg.bLength = USB_DT_CONFIG_SIZE;
		if (type == USB_DT_OTHER_SPEED_CONFIG)
			cfg.bDescriptorType =  USB_DT_OTHER_SPEED_CONFIG;
		else
			cfg.bDescriptorType = USB_DT_CONFIG;
		cfg.wTotalLength = ptr - start;
		cfg.bNumInterfaces = ifc_count;
		cfg.bConfigurationValue = 1;
		cfg.iConfiguration = 0;
		cfg.bmAttributes = USB_CONFIG_ATT_ONE |
			ui->selfpowered << USB_CONFIG_ATT_SELFPOWER_POS |
			USB_REMOTE_WAKEUP_SUPPORT << USB_CONFIG_ATT_WAKEUP_POS;
		cfg.bMaxPower = ui->maxpower;

		memcpy(start, &cfg, USB_DT_CONFIG_SIZE);

		req->length = ptr - start;
		return 0;
	}

	if (type == USB_DT_STRING) {
		char *buffer = req->buf;

		buffer[0] = 0;
		if (id > ui->strdesc_index)
			return -1;
		 if (id == STRING_LANGUAGE_ID)
			memcpy(buffer, str_lang_desc, str_lang_desc[0]);
		 else
			copy_string_descriptor(ui->strdesc[id], buffer);

		if (buffer[0]) {
			req->length = buffer[0];
			return 0;
		} else
			return -1;
	}
	return -1;
}

/*****Gadget Framework Functions***/
struct device *usb_get_device(void)
{
	if (the_usb_info) {
		if (the_usb_info->pdev)
			return &(the_usb_info->pdev->dev);
	}
	return NULL;
}
EXPORT_SYMBOL(usb_get_device);

int usb_ept_cancel_xfer(struct usb_endpoint *ept, struct usb_request *_req)
{
	struct usb_info 	*ui = the_usb_info;
	struct msm_request      *req = to_msm_request(_req);
	struct msm_request 	*temp_req, *prev_req;
	unsigned long		flags;

	if (!(ui && req && ept->req))
		return -EINVAL;

	spin_lock_irqsave(&ui->lock, flags);
	if (req->busy) {
		req->req.status = 0;
		req->busy = 0;

		/* See if the request is the first request in the ept queue */
		if (ept->req == req) {
			/* Stop the transfer */
			do {
				writel((1 << ept->bit), USB_ENDPTFLUSH);
				while (readl(USB_ENDPTFLUSH) & (1 << ept->bit))
					udelay(100);
			} while (readl(USB_ENDPTSTAT) & (1 << ept->bit));
			if (!req->next)
				ept->last = NULL;
			ept->req = req->next;
			ept->head->next = req->item->next;
			goto cancel_req;
		}
		/* Request could be in the middle of ept queue */
		prev_req = temp_req = ept->req;
		do {
			if (req == temp_req) {
				if (req->live) {
					/* Stop the transfer */
					do {
						writel((1 << ept->bit),
							USB_ENDPTFLUSH);
						while (readl(USB_ENDPTFLUSH) &
							(1 << ept->bit))
							udelay(100);
					} while (readl(USB_ENDPTSTAT) &
						(1 << ept->bit));
				}
				prev_req->next = temp_req->next;
				prev_req->item->next = temp_req->item->next;
				if (!req->next)
					ept->last = prev_req;
				goto cancel_req;
			}
			prev_req = temp_req;
			temp_req = temp_req->next;
		} while (temp_req != NULL);
		goto error;
cancel_req:
	if (req->live) {
		/* prepare the transaction descriptor item for the hardware */
		req->item->next = TERMINATE;
		req->item->info = 0;
		req->live = 0;
		dma_unmap_single(NULL, req->dma, req->req.length,
				(ept->flags & EPT_FLAG_IN) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE);
		/* Reprime the endpoint for the remaining transfers */
		if (ept->req) {
			temp_req = ept->req;
			while (temp_req != NULL) {
				temp_req->live = 0;
				temp_req = temp_req->next;
			}
			usb_ept_start(ept);
		}
	} else
		dma_unmap_single(NULL, req->dma, req->req.length,
				(ept->flags & EPT_FLAG_IN) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE);
	spin_unlock_irqrestore(&ui->lock, flags);
	return 0;
	}
error:
	spin_unlock_irqrestore(&ui->lock, flags);
	return -EINVAL;
}
EXPORT_SYMBOL(usb_ept_cancel_xfer);

int usb_ept_set_halt(struct usb_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	int in = ept->flags & EPT_FLAG_IN;
	unsigned n;

	if (ui->in_lpm) {
		pr_err("%s: controller is in lpm, cannot proceed\n", __func__);
		return -1;
	}

	ept->ept_halted = 1;

	n = readl(USB_ENDPTCTRL(ept->num));

	if (in)
		n |= CTRL_TXS;
	else
		n |= CTRL_RXS;

	writel(n, USB_ENDPTCTRL(ept->num));

	return 0;
}
EXPORT_SYMBOL(usb_ept_set_halt);

int usb_ept_clear_halt(struct usb_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	int in = ept->flags & EPT_FLAG_IN;
	unsigned n;

	if (ui->in_lpm) {
		pr_err("%s: controller is in lpm, cannot proceed\n", __func__);
		return -1;
	}

	if (ept->ept_halted)
		ept->ept_halted = 0;

	n = readl(USB_ENDPTCTRL(ept->num));

	/*clear stall bit and set data toggle bit*/
	if (in) {
		n &= (~CTRL_TXS);
		n |= (CTRL_TXR);
	} else {
		n &= ~(CTRL_RXS);
		n |= (CTRL_RXR);
	}

	writel(n, USB_ENDPTCTRL(ept->num));

	return 0;
}
EXPORT_SYMBOL(usb_ept_clear_halt);

int usb_ept_is_stalled(struct usb_endpoint *ept)
{
	struct usb_info *ui = ept->ui;
	int in = ept->flags & EPT_FLAG_IN;
	unsigned n;

	n = readl(USB_ENDPTCTRL(ept->num));

	if (in && (n & CTRL_TXS))
		return 1;
	else if (n & CTRL_RXS)
		return 1;
	return 0;
}

void usb_ept_fifo_flush(struct usb_endpoint *ept)
{
	flush_endpoint(ept);
}
EXPORT_SYMBOL(usb_ept_fifo_flush);

struct usb_function *usb_ept_get_function(struct usb_endpoint *ept)
{
	return NULL;
}
EXPORT_SYMBOL(usb_ept_get_function);


void usb_free_endpoint_all_req(struct usb_endpoint *ep)
{
	struct msm_request *temp;
	struct msm_request *req;
	if (!ep)
		return;
	req = ep->req;
	while (req) {
		temp = req->next;
		req->busy = 0;
		if (&req->req)
			usb_ept_free_req(ep, &req->req);
		req = temp;
	}
}
EXPORT_SYMBOL(usb_free_endpoint_all_req);

int usb_function_unregister(struct usb_function *func)
{
	struct usb_info *ui = the_usb_info;
	int i;
	struct usb_function_info *fi;
	unsigned long flags;

	if (!func)
		return -EINVAL;

	fi = usb_find_function(func->name);
	if (!fi)
		return -EINVAL;

	if (ui->running) {
		disable_irq(ui->irq);
		spin_lock_irqsave(&ui->lock, flags);
		ui->running = 0;
		ui->online = 0;
		ui->bound = 0;
		spin_unlock_irqrestore(&ui->lock, flags);
		usb_uninit(ui);
		/* we should come out of lpm to access registers */
		if (ui->in_lpm) {
			if (PHY_TYPE(ui->phy_info) == USB_PHY_EXTERNAL) {
				disable_irq(ui->gpio_irq[0]);
				disable_irq(ui->gpio_irq[1]);
			}
			usb_lpm_exit(ui);
			if (cancel_work_sync(&ui->li.wakeup_phy))
				usb_lpm_wakeup_phy(NULL);
			ui->in_lpm = 0;
		}
		/* disable usb and session valid interrupts */
		writel(0, USB_USBINTR);
		writel(readl(USB_OTGSC) & ~OTGSC_BSVIE, USB_OTGSC);

		/* stop the controller */
		usb_disable_pullup(ui);
		msleep(100);
		enable_irq(ui->irq);
	}

	pr_info("%s: func->name = %s\n", __func__, func->name);

	ui->composition = NULL;

	if (func->configure)
		func->configure(0, func->context);
	if (func->unbind)
		func->unbind(func->context);

	list_del(&fi->list);
	for (i = 0; i < ui->num_funcs; i++)
		if (fi == ui->func[i])
			ui->func[i] = NULL;
	kfree(fi);
	return 0;
}
EXPORT_SYMBOL(usb_function_unregister);

MODULE_LICENSE("GPL");

