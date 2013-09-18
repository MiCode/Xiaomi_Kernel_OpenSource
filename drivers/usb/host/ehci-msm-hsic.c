/* ehci-msm-hsic.c - HSUSB Host Controller Driver Implementation
 *
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * Partly derived from ehci-fsl.c and ehci-hcd.c
 * Copyright (c) 2000-2004 by David Brownell
 * Copyright (c) 2005 MontaVista Software
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/wakelock.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/msm_hsusb_hw.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/pm_qos.h>
#include <linux/dma-mapping.h>

#include <mach/msm_bus.h>
#include <mach/clk.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>
#include <mach/rpm-regulator.h>

#include "ehci.h"

#include "hbm.c"

#define MSM_USB_BASE (hcd->regs)
#define USB_REG_START_OFFSET 0x90
#define USB_REG_END_OFFSET 0x250

static struct workqueue_struct  *ehci_wq;
struct ehci_timer {
#define GPT_LD(p)	((p) & 0x00FFFFFF)
	u32	gptimer0_ld;
#define GPT_RUN		BIT(31)
#define GPT_RESET	BIT(30)
#define GPT_MODE	BIT(24)
#define GPT_CNT(p)	((p) & 0x00FFFFFF)
	u32	gptimer0_ctrl;

	u32	gptimer1_ld;
	u32	gptimer1_ctrl;
};

struct msm_hsic_hcd {
	struct ehci_hcd		ehci;
	spinlock_t		wakeup_lock;
	struct device		*dev;
	struct clk		*ahb_clk;
	struct clk		*core_clk;
	struct clk		*alt_core_clk;
	struct clk		*phy_clk;
	struct clk		*cal_clk;
	struct clk		*inactivity_clk;
	struct regulator	*hsic_vddcx;
	struct regulator	*hsic_gdsc;
	atomic_t		async_int;
	atomic_t                in_lpm;
	struct wake_lock	wlock;
	int			peripheral_status_irq;
	int			wakeup_irq;
	bool			wakeup_irq_enabled;
	int			async_irq;
	uint32_t		async_int_cnt;
	atomic_t		pm_usage_cnt;
	uint32_t		bus_perf_client;
	uint32_t		wakeup_int_cnt;
	enum usb_vdd_type	vdd_type;

	struct work_struct	bus_vote_w;
	bool			bus_vote;

	/* gp timer */
	struct ehci_timer __iomem *timer;
	struct completion	gpt0_completion;
	struct completion	rt_completion;
	int			resume_status;
	int			resume_again;
	int			bus_reset;
	int			reset_again;

	struct pm_qos_request pm_qos_req_dma;
	unsigned		enable_hbm:1;
};

struct msm_hsic_hcd *__mehci;

static bool debug_bus_voting_enabled = true;
static u64 ehci_msm_hsic_dma_mask = DMA_BIT_MASK(32);

static struct platform_driver ehci_msm_hsic_driver;

static unsigned int enable_payload_log = 1;
module_param(enable_payload_log, uint, S_IRUGO | S_IWUSR);
static unsigned int enable_dbg_log = 1;
module_param(enable_dbg_log, uint, S_IRUGO | S_IWUSR);
/*by default log ep0 and efs sync ep*/
static unsigned int ep_addr_rxdbg_mask = 9;
module_param(ep_addr_rxdbg_mask, uint, S_IRUGO | S_IWUSR);
static unsigned int ep_addr_txdbg_mask = 9;
module_param(ep_addr_txdbg_mask, uint, S_IRUGO | S_IWUSR);

/* Maximum debug message length */
#define DBG_MSG_LEN   128UL

/* Maximum number of messages */
#define DBG_MAX_MSG   256UL

#define TIME_BUF_LEN  20
#define HEX_DUMP_LEN  72

enum event_type {
	EVENT_UNDEF = -1,
	URB_SUBMIT,
	URB_COMPLETE,
	EVENT_NONE,
};

#define EVENT_STR_LEN	5

static enum event_type str_to_event(const char *name)
{
	if (!strncasecmp("S", name, EVENT_STR_LEN))
		return URB_SUBMIT;
	if (!strncasecmp("C", name, EVENT_STR_LEN))
		return URB_COMPLETE;
	if (!strncasecmp("", name, EVENT_STR_LEN))
		return EVENT_NONE;

	return EVENT_UNDEF;
}

/*log ep0 activity*/
static struct {
	char     (buf[DBG_MAX_MSG])[DBG_MSG_LEN];   /* buffer */
	unsigned idx;   /* index */
	rwlock_t lck;   /* lock */
} dbg_hsic_ctrl = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

static struct {
	char     (buf[DBG_MAX_MSG])[DBG_MSG_LEN];   /* buffer */
	unsigned idx;   /* index */
	rwlock_t lck;   /* lock */
} dbg_hsic_data = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

/**
 * dbg_inc: increments debug event index
 * @idx: buffer index
 */
static void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DBG_MAX_MSG-1);
}

/*get_timestamp - returns time of day in us */
static char *get_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000)/1000;
	scnprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu] ", (unsigned long)t,
		nanosec_rem);
	return tbuf;
}

static int allow_dbg_log(struct urb *urb, int ep_addr)
{
	int dir, num;

	dir = usb_urb_dir_in(urb) ? USB_DIR_IN : USB_DIR_OUT;
	num = ep_addr & ~USB_DIR_IN;
	num = 1 << num;

	if ((dir == USB_DIR_IN) && (num & ep_addr_rxdbg_mask))
		return 1;
	if ((dir == USB_DIR_OUT) && (num & ep_addr_txdbg_mask))
		return 1;

	return 0;
}

static char *
get_hex_data(char *dbuf, struct urb *urb, int event, int status, size_t max_len)
{
	char *ubuf = urb->transfer_buffer;
	size_t len = event ? \
		urb->actual_length : urb->transfer_buffer_length;

	if (status == -EINPROGRESS)
		status = 0;

	/*Only dump ep in successful completions and epout submissions*/
	if (len && !status && ((usb_urb_dir_in(urb) && event) ||
			(usb_urb_dir_out(urb) && !event))) {
		if (len >= max_len)
			len = max_len;
		hex_dump_to_buffer(ubuf, len, 32, 4, dbuf, HEX_DUMP_LEN, 0);
	} else {
		dbuf = "";
	}

	return dbuf;
}

static void dbg_log_event(struct urb *urb, char * event, unsigned extra)
{
	unsigned long flags;
	int ep_addr;
	char tbuf[TIME_BUF_LEN];
	char dbuf[HEX_DUMP_LEN];

	if (!enable_dbg_log)
		return;

	if (!urb) {
		write_lock_irqsave(&dbg_hsic_ctrl.lck, flags);
		scnprintf(dbg_hsic_ctrl.buf[dbg_hsic_ctrl.idx], DBG_MSG_LEN,
			"%s: %s : %u", get_timestamp(tbuf), event, extra);
		dbg_inc(&dbg_hsic_ctrl.idx);
		write_unlock_irqrestore(&dbg_hsic_ctrl.lck, flags);
		return;
	}

	ep_addr = urb->ep->desc.bEndpointAddress;
	if (!allow_dbg_log(urb, ep_addr))
		return;

	if ((ep_addr & 0x0f) == 0x0) {
		/*submit event*/
		if (!str_to_event(event)) {
			write_lock_irqsave(&dbg_hsic_ctrl.lck, flags);
			scnprintf(dbg_hsic_ctrl.buf[dbg_hsic_ctrl.idx],
				DBG_MSG_LEN, "%s: [%s : %p]:[%s] "
				  "%02x %02x %04x %04x %04x  %u %d %s",
				  get_timestamp(tbuf), event, urb,
				  usb_urb_dir_in(urb) ? "in" : "out",
				  urb->setup_packet[0], urb->setup_packet[1],
				  (urb->setup_packet[3] << 8) |
				  urb->setup_packet[2],
				  (urb->setup_packet[5] << 8) |
				  urb->setup_packet[4],
				  (urb->setup_packet[7] << 8) |
				  urb->setup_packet[6],
				  urb->transfer_buffer_length, extra,
				  enable_payload_log ? get_hex_data(dbuf, urb,
				  str_to_event(event), extra, 16) : "");

			dbg_inc(&dbg_hsic_ctrl.idx);
			write_unlock_irqrestore(&dbg_hsic_ctrl.lck, flags);
		} else {
			write_lock_irqsave(&dbg_hsic_ctrl.lck, flags);
			scnprintf(dbg_hsic_ctrl.buf[dbg_hsic_ctrl.idx],
				DBG_MSG_LEN, "%s: [%s : %p]:[%s] %u %d %s",
				  get_timestamp(tbuf), event, urb,
				  usb_urb_dir_in(urb) ? "in" : "out",
				  urb->actual_length, extra,
				  enable_payload_log ? get_hex_data(dbuf, urb,
				  str_to_event(event), extra, 16) : "");

			dbg_inc(&dbg_hsic_ctrl.idx);
			write_unlock_irqrestore(&dbg_hsic_ctrl.lck, flags);
		}
	} else {
		write_lock_irqsave(&dbg_hsic_data.lck, flags);
		scnprintf(dbg_hsic_data.buf[dbg_hsic_data.idx], DBG_MSG_LEN,
			  "%s: [%s : %p]:ep%d[%s]  %u %d %s",
			  get_timestamp(tbuf), event, urb, ep_addr & 0x0f,
			  usb_urb_dir_in(urb) ? "in" : "out",
			  str_to_event(event) ? urb->actual_length :
			  urb->transfer_buffer_length, extra,
			  enable_payload_log ? get_hex_data(dbuf, urb,
				  str_to_event(event), extra, 32) : "");

		dbg_inc(&dbg_hsic_data.idx);
		write_unlock_irqrestore(&dbg_hsic_data.lck, flags);
	}
}

static inline struct msm_hsic_hcd *hcd_to_hsic(struct usb_hcd *hcd)
{
	return (struct msm_hsic_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *hsic_to_hcd(struct msm_hsic_hcd *mehci)
{
	return container_of((void *) mehci, struct usb_hcd, hcd_priv);
}

static void dump_hsic_regs(struct usb_hcd *hcd)
{
	int i;
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	if (atomic_read(&mehci->in_lpm))
		return;

	for (i = USB_REG_START_OFFSET; i <= USB_REG_END_OFFSET; i += 0x10)
		pr_info("%p: %08x\t%08x\t%08x\t%08x\n", hcd->regs + i,
				readl_relaxed(hcd->regs + i),
				readl_relaxed(hcd->regs + i + 4),
				readl_relaxed(hcd->regs + i + 8),
				readl_relaxed(hcd->regs + i + 0xc));
}

#define ULPI_IO_TIMEOUT_USEC	(10 * 1000)

#define USB_PHY_VDD_DIG_VOL_NONE	0 /*uV */
#define USB_PHY_VDD_DIG_VOL_MIN		945000 /* uV */
#define USB_PHY_VDD_DIG_VOL_MAX		1320000 /* uV */

#define HSIC_DBG1_REG		0x38

static int vdd_val[VDD_TYPE_MAX][VDD_VAL_MAX] = {
		{   /* VDD_CX CORNER Voting */
			[VDD_NONE]	= RPM_VREG_CORNER_NONE,
			[VDD_MIN]	= RPM_VREG_CORNER_NOMINAL,
			[VDD_MAX]	= RPM_VREG_CORNER_HIGH,
		},
		{   /* VDD_CX Voltage Voting */
			[VDD_NONE]	= USB_PHY_VDD_DIG_VOL_NONE,
			[VDD_MIN]	= USB_PHY_VDD_DIG_VOL_MIN,
			[VDD_MAX]	= USB_PHY_VDD_DIG_VOL_MAX,
		},
};

static int msm_hsic_init_vddcx(struct msm_hsic_hcd *mehci, int init)
{
	int ret = 0;
	int none_vol, min_vol, max_vol;
	u32 tmp[3];
	int len = 0;

	if (!mehci->hsic_vddcx) {
		mehci->vdd_type = VDDCX_CORNER;
		mehci->hsic_vddcx = devm_regulator_get(mehci->dev,
			"hsic_vdd_dig");
		if (IS_ERR(mehci->hsic_vddcx)) {
			mehci->hsic_vddcx = devm_regulator_get(mehci->dev,
				"HSIC_VDDCX");
			if (IS_ERR(mehci->hsic_vddcx)) {
				dev_err(mehci->dev, "unable to get hsic vddcx\n");
				return PTR_ERR(mehci->hsic_vddcx);
			}
			mehci->vdd_type = VDDCX;
		}

		if (mehci->dev->of_node) {
			of_get_property(mehci->dev->of_node,
					"hsic,vdd-voltage-level",
					&len);
			if (len == sizeof(tmp)) {
				of_property_read_u32_array(mehci->dev->of_node,
						"hsic,vdd-voltage-level",
						tmp, len/sizeof(*tmp));
				vdd_val[mehci->vdd_type][VDD_NONE] = tmp[0];
				vdd_val[mehci->vdd_type][VDD_MIN] = tmp[1];
				vdd_val[mehci->vdd_type][VDD_MAX] = tmp[2];
			} else {
				dev_dbg(mehci->dev, "Use default vdd config\n");
			}
		}
	}

	none_vol = vdd_val[mehci->vdd_type][VDD_NONE];
	min_vol = vdd_val[mehci->vdd_type][VDD_MIN];
	max_vol = vdd_val[mehci->vdd_type][VDD_MAX];

	if (!init)
		goto disable_reg;

	ret = regulator_set_voltage(mehci->hsic_vddcx, min_vol, max_vol);
	if (ret) {
		dev_err(mehci->dev, "unable to set the voltage"
				"for hsic vddcx\n");
		return ret;
	}

	ret = regulator_enable(mehci->hsic_vddcx);
	if (ret) {
		dev_err(mehci->dev, "unable to enable hsic vddcx\n");
		goto reg_enable_err;
	}

	return 0;

disable_reg:
	regulator_disable(mehci->hsic_vddcx);
reg_enable_err:
	regulator_set_voltage(mehci->hsic_vddcx, none_vol, max_vol);

	return ret;

}

/* Global Distributed Switch Controller (GDSC) init */
static int msm_hsic_init_gdsc(struct msm_hsic_hcd *mehci, int init)
{
	int ret = 0;

	if (IS_ERR(mehci->hsic_gdsc))
		return 0;

	if (!mehci->hsic_gdsc) {
		mehci->hsic_gdsc = devm_regulator_get(mehci->dev,
			"HSIC_GDSC");
		if (IS_ERR(mehci->hsic_gdsc))
			return 0;
	}

	if (init) {
		ret = regulator_enable(mehci->hsic_gdsc);
		if (ret) {
			dev_err(mehci->dev, "unable to enable hsic gdsc\n");
			return ret;
		}
	} else {
		regulator_disable(mehci->hsic_gdsc);
	}

	return 0;

}

static int __maybe_unused ulpi_read(struct msm_hsic_hcd *mehci, u32 reg)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0;

	/* initiate read operation */
	writel_relaxed(ULPI_RUN | ULPI_READ | ULPI_ADDR(reg),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		dev_err(mehci->dev, "ulpi_read: timeout ULPI_VIEWPORT: %08x\n",
				readl_relaxed(USB_ULPI_VIEWPORT));
		dev_err(mehci->dev, "PORTSC: %08x USBCMD: %08x FRINDEX: %08x\n",
				readl_relaxed(USB_PORTSC),
				readl_relaxed(USB_USBCMD),
				readl_relaxed(USB_FRINDEX));

		/*frame counter increments afte 125us*/
		udelay(130);
		dev_err(mehci->dev, "ulpi_read: FRINDEX: %08x\n",
				readl_relaxed(USB_FRINDEX));
		return -ETIMEDOUT;
	}

	return ULPI_DATA_READ(readl_relaxed(USB_ULPI_VIEWPORT));
}

static int ulpi_write(struct msm_hsic_hcd *mehci, u32 val, u32 reg)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0;

	/* initiate write operation */
	writel_relaxed(ULPI_RUN | ULPI_WRITE |
	       ULPI_ADDR(reg) | ULPI_DATA(val),
	       USB_ULPI_VIEWPORT);

	/* wait for completion */
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_RUN))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		dev_err(mehci->dev, "ulpi_write: timeout ULPI_VIEWPORT: %08x\n",
				readl_relaxed(USB_ULPI_VIEWPORT));
		dev_err(mehci->dev, "PORTSC: %08x USBCMD: %08x FRINDEX: %08x\n",
				readl_relaxed(USB_PORTSC),
				readl_relaxed(USB_USBCMD),
				readl_relaxed(USB_FRINDEX));

		/*frame counter increments afte 125us*/
		udelay(130);
		dev_err(mehci->dev, "ulpi_write: FRINDEX: %08x\n",
				readl_relaxed(USB_FRINDEX));
		return -ETIMEDOUT;
	}

	return 0;
}

static int msm_hsic_config_gpios(struct msm_hsic_hcd *mehci, int gpio_en)
{
	int rc = 0;
	struct msm_hsic_host_platform_data *pdata;
	static int gpio_status;

	pdata = mehci->dev->platform_data;

	if (!pdata || !pdata->strobe || !pdata->data)
		return rc;

	if (gpio_status == gpio_en)
		return 0;

	gpio_status = gpio_en;

	if (!gpio_en)
		goto free_gpio;

	rc = gpio_request(pdata->strobe, "HSIC_STROBE_GPIO");
	if (rc < 0) {
		dev_err(mehci->dev, "gpio request failed for HSIC STROBE\n");
		return rc;
	}

	rc = gpio_request(pdata->data, "HSIC_DATA_GPIO");
	if (rc < 0) {
		dev_err(mehci->dev, "gpio request failed for HSIC DATA\n");
		goto free_strobe;
	}

	return 0;

free_gpio:
	gpio_free(pdata->data);
free_strobe:
	gpio_free(pdata->strobe);

	return rc;
}

static void msm_hsic_clk_reset(struct msm_hsic_hcd *mehci)
{
	int ret;

	/* alt_core_clk exists in targets that do not use asynchronous reset */
	if (!IS_ERR(mehci->alt_core_clk)) {
		ret = clk_reset(mehci->core_clk, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(mehci->dev, "hsic clk assert failed:%d\n", ret);
			return;
		}

		/* Since a hw bug, turn off the clock before complete reset */
		clk_disable(mehci->core_clk);

		ret = clk_reset(mehci->core_clk, CLK_RESET_DEASSERT);
		if (ret)
			dev_err(mehci->dev, "hsic clk deassert failed:%d\n",
					ret);

		usleep_range(10000, 12000);

		clk_enable(mehci->core_clk);
	} else {
		/* Using asynchronous block reset to the hardware */
		clk_disable_unprepare(mehci->core_clk);
		clk_disable_unprepare(mehci->phy_clk);
		clk_disable_unprepare(mehci->cal_clk);
		clk_disable_unprepare(mehci->ahb_clk);
		if (!IS_ERR(mehci->inactivity_clk))
			clk_disable_unprepare(mehci->inactivity_clk);

		ret = clk_reset(mehci->core_clk, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(mehci->dev, "hsic clk assert failed:%d\n", ret);
			return;
		}
		usleep_range(10000, 12000);

		ret = clk_reset(mehci->core_clk, CLK_RESET_DEASSERT);
		if (ret)
			dev_err(mehci->dev, "hsic clk deassert failed:%d\n",
					ret);
		/*
		 * Required delay between the deassertion and
		 *  clock enablement.
		*/
		ndelay(200);
		clk_prepare_enable(mehci->core_clk);
		clk_prepare_enable(mehci->phy_clk);
		clk_prepare_enable(mehci->cal_clk);
		clk_prepare_enable(mehci->ahb_clk);
		if (!IS_ERR(mehci->inactivity_clk))
			clk_prepare_enable(mehci->inactivity_clk);
	}
}

#define HSIC_STROBE_GPIO_PAD_CTL	(MSM_TLMM_BASE+0x20C0)
#define HSIC_DATA_GPIO_PAD_CTL		(MSM_TLMM_BASE+0x20C4)
#define HSIC_CAL_PAD_CTL       (MSM_TLMM_BASE+0x20C8)
#define HSIC_LV_MODE		0x04
#define HSIC_PAD_CALIBRATION	0xA8
#define HSIC_GPIO_PAD_VAL	0x0A0AAA10
#define LINK_RESET_TIMEOUT_USEC		(250 * 1000)

static void msm_hsic_phy_reset(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);

	msm_hsic_clk_reset(mehci);

	/* select ulpi phy */
	writel_relaxed(0x80000000, USB_PORTSC);
	mb();
}

static int msm_hsic_start(struct msm_hsic_hcd *mehci)
{
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;
	int ret;
	void __iomem *reg;

	if (pdata && pdata->resume_gpio) {
		ret = gpio_request(pdata->resume_gpio, "HSIC_RESUME_GPIO");
		if (ret < 0) {
			dev_err(mehci->dev,
				"gpio req failed for hsic resume:%d\n", ret);
			pdata->resume_gpio = 0;
		}
	}

	/* HSIC init sequence when HSIC signals (Strobe/Data) are
	routed via GPIOs */
	if (pdata && pdata->strobe && pdata->data) {

		if (!pdata->ignore_cal_pad_config) {
			/* Enable LV_MODE in HSIC_CAL_PAD_CTL register */
			writel_relaxed(HSIC_LV_MODE, HSIC_CAL_PAD_CTL);
			mb();
		}

		/*set periodic calibration interval to ~2.048sec in
		  HSIC_IO_CAL_REG */
		ulpi_write(mehci, 0xFF, 0x33);

		/* Enable periodic IO calibration in HSIC_CFG register */
		ulpi_write(mehci, HSIC_PAD_CALIBRATION, 0x30);

		/* Configure GPIO pins for HSIC functionality mode */
		ret = msm_hsic_config_gpios(mehci, 1);
		if (ret) {
			dev_err(mehci->dev, " gpio configuarion failed\n");
			goto free_resume_gpio;
		}
		if (pdata->strobe_pad_offset) {
			/* Set CORE_CTL_EN in STROBE GPIO PAD_CTL register */
			reg = MSM_TLMM_BASE + pdata->strobe_pad_offset;
			writel_relaxed(readl_relaxed(reg) | 0x2000000, reg);
		} else {
			/* Set LV_MODE=0x1 and DCC=0x2 in STROBE GPIO PAD_CTL */
			reg = HSIC_STROBE_GPIO_PAD_CTL;
			writel_relaxed(HSIC_GPIO_PAD_VAL, reg);
		}

		if (pdata->data_pad_offset) {
			/* Set CORE_CTL_EN in HSIC_DATA GPIO PAD_CTL register */
			reg = MSM_TLMM_BASE + pdata->data_pad_offset;
			writel_relaxed(readl_relaxed(reg) | 0x2000000, reg);
		} else {
			/* Set LV_MODE=0x1 and DCC=0x2 in STROBE GPIO PAD_CTL */
			reg = HSIC_DATA_GPIO_PAD_CTL;
			writel_relaxed(HSIC_GPIO_PAD_VAL, reg);
		}

		mb();

		/* Enable HSIC mode in HSIC_CFG register */
		ulpi_write(mehci, 0x01, 0x31);
	} else {
		/* HSIC init sequence when HSIC signals (Strobe/Data) are routed
		via dedicated I/O */

		/* programmable length of connect signaling (33.2ns) */
		ret = ulpi_write(mehci, 3, HSIC_DBG1_REG);
		if (ret) {
			pr_err("%s: Unable to program length of connect "
			      "signaling\n", __func__);
		}

		/*set periodic calibration interval to ~2.048sec in
		  HSIC_IO_CAL_REG */
		ulpi_write(mehci, 0xFF, 0x33);

		/* Enable HSIC mode in HSIC_CFG register */
		ulpi_write(mehci, 0xA9, 0x30);
	}

	/*disable auto resume*/
	ulpi_write(mehci, ULPI_IFC_CTRL_AUTORESUME, ULPI_CLR(ULPI_IFC_CTRL));

	return 0;

free_resume_gpio:
	if (pdata && pdata->resume_gpio)
		gpio_free(pdata->resume_gpio);

	return ret;
}

#define PHY_SUSPEND_TIMEOUT_USEC	(500 * 1000)
#define PHY_RESUME_TIMEOUT_USEC		(100 * 1000)

#ifdef CONFIG_PM_SLEEP
static int msm_hsic_reset(struct msm_hsic_hcd *mehci)
{
	/* reset HSIC phy */
	msm_hsic_phy_reset(mehci);

	/* HSIC init procedure (caliberation) */
	return msm_hsic_start(mehci);
}

static int msm_hsic_suspend(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0, ret;
	u32 val;
	int none_vol, max_vol;
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;

	if (atomic_read(&mehci->in_lpm)) {
		dev_dbg(mehci->dev, "%s called in lpm\n", __func__);
		return 0;
	}

	disable_irq(hcd->irq);

	/* make sure we don't race against a remote wakeup */
	if (test_bit(HCD_FLAG_WAKEUP_PENDING, &hcd->flags) ||
	    readl_relaxed(USB_PORTSC) & PORT_RESUME) {
		dev_dbg(mehci->dev, "wakeup pending, aborting suspend\n");
		enable_irq(hcd->irq);
		return -EBUSY;
	}

	if (pdata->consider_ipa_handshake) {
		dev_dbg(mehci->dev, "%s:Wait for resources release\n",
			__func__);
		if (!msm_bam_hsic_lpm_ok()) {
			dev_dbg(mehci->dev, "%s:Prod+Cons not released\n",
			__func__);
			enable_irq(hcd->irq);
			return -EBUSY;
		}
		dev_dbg(mehci->dev, "%s:Prod+Cons resources released\n",
			__func__);
	}

	/*
	 * PHY may take some time or even fail to enter into low power
	 * mode (LPM). Hence poll for 500 msec and reset the PHY and link
	 * in failure case.
	 */
	val = readl_relaxed(USB_PORTSC);
	val &= ~PORT_RWC_BITS;
	val |= PORTSC_PHCD;
	writel_relaxed(val, USB_PORTSC);
	while (cnt < PHY_SUSPEND_TIMEOUT_USEC) {
		if (readl_relaxed(USB_PORTSC) & PORTSC_PHCD)
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= PHY_SUSPEND_TIMEOUT_USEC) {
		dev_err(mehci->dev, "Unable to suspend PHY\n");
		msm_hsic_config_gpios(mehci, 0);
		msm_hsic_reset(mehci);
	}

	/*
	 * PHY has capability to generate interrupt asynchronously in low
	 * power mode (LPM). This interrupt is level triggered. So USB IRQ
	 * line must be disabled till async interrupt enable bit is cleared
	 * in USBCMD register. Assert STP (ULPI interface STOP signal) to
	 * block data communication from PHY.  Enable asynchronous interrupt
	 * only when wakeup gpio IRQ is not present.
	 */
	if (mehci->wakeup_irq)
		writel_relaxed(readl_relaxed(USB_USBCMD) |
				ULPI_STP_CTRL, USB_USBCMD);
	else
		writel_relaxed(readl_relaxed(USB_USBCMD) | ASYNC_INTR_CTRL |
				ULPI_STP_CTRL, USB_USBCMD);

	/*
	 * Ensure that hardware is put in low power mode before
	 * clocks are turned OFF and VDD is allowed to minimize.
	 */
	mb();

	clk_disable_unprepare(mehci->core_clk);
	clk_disable_unprepare(mehci->phy_clk);
	clk_disable_unprepare(mehci->cal_clk);
	clk_disable_unprepare(mehci->ahb_clk);
	if (!IS_ERR(mehci->inactivity_clk))
		clk_disable_unprepare(mehci->inactivity_clk);

	none_vol = vdd_val[mehci->vdd_type][VDD_NONE];
	max_vol = vdd_val[mehci->vdd_type][VDD_MAX];

	ret = regulator_set_voltage(mehci->hsic_vddcx, none_vol, max_vol);
	if (ret < 0)
		dev_err(mehci->dev, "unable to set vddcx voltage for VDD MIN\n");

	if (mehci->bus_perf_client && debug_bus_voting_enabled) {
		mehci->bus_vote = false;
		queue_work(ehci_wq, &mehci->bus_vote_w);
	}

	atomic_set(&mehci->in_lpm, 1);
	enable_irq(hcd->irq);

	if (mehci->wakeup_irq) {
		mehci->wakeup_irq_enabled = 1;
		enable_irq_wake(mehci->wakeup_irq);
		enable_irq(mehci->wakeup_irq);
	}

	if (pdata && pdata->standalone_latency)
		pm_qos_update_request(&mehci->pm_qos_req_dma,
			PM_QOS_DEFAULT_VALUE);

	wake_unlock(&mehci->wlock);

	dev_info(mehci->dev, "HSIC-USB in low power mode\n");

	return 0;
}

static int msm_hsic_resume(struct msm_hsic_hcd *mehci)
{
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	int cnt = 0, ret;
	unsigned temp;
	int min_vol, max_vol;
	unsigned long flags;
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;

	if (!atomic_read(&mehci->in_lpm)) {
		dev_dbg(mehci->dev, "%s called in !in_lpm\n", __func__);
		return 0;
	}

	if (pdata->consider_ipa_handshake) {
		dev_dbg(mehci->dev, "%s:Wait for producer resource\n",
			__func__);
		msm_bam_wait_for_hsic_prod_granted();
		dev_dbg(mehci->dev, "%s:Producer resource obtained\n",
			__func__);
	}

	/* Handles race with Async interrupt */
	disable_irq(hcd->irq);

	if (pdata && pdata->standalone_latency)
		pm_qos_update_request(&mehci->pm_qos_req_dma,
			pdata->standalone_latency + 1);

	if (mehci->wakeup_irq) {
		spin_lock_irqsave(&mehci->wakeup_lock, flags);
		if (mehci->wakeup_irq_enabled) {
			disable_irq_wake(mehci->wakeup_irq);
			disable_irq_nosync(mehci->wakeup_irq);
			mehci->wakeup_irq_enabled = 0;
		}
		spin_unlock_irqrestore(&mehci->wakeup_lock, flags);
	}

	wake_lock(&mehci->wlock);

	if (mehci->bus_perf_client && debug_bus_voting_enabled) {
		mehci->bus_vote = true;
		queue_work(ehci_wq, &mehci->bus_vote_w);
	}

	min_vol = vdd_val[mehci->vdd_type][VDD_MIN];
	max_vol = vdd_val[mehci->vdd_type][VDD_MAX];

	ret = regulator_set_voltage(mehci->hsic_vddcx, min_vol, max_vol);
	if (ret < 0)
		dev_err(mehci->dev, "unable to set nominal vddcx voltage (no VDD MIN)\n");

	clk_prepare_enable(mehci->core_clk);
	clk_prepare_enable(mehci->phy_clk);
	clk_prepare_enable(mehci->cal_clk);
	clk_prepare_enable(mehci->ahb_clk);
	if (!IS_ERR(mehci->inactivity_clk))
		clk_prepare_enable(mehci->inactivity_clk);

	temp = readl_relaxed(USB_USBCMD);
	temp &= ~ASYNC_INTR_CTRL;
	temp &= ~ULPI_STP_CTRL;
	writel_relaxed(temp, USB_USBCMD);

	if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD))
		goto skip_phy_resume;

	temp = readl_relaxed(USB_PORTSC);
	temp &= ~(PORT_RWC_BITS | PORTSC_PHCD);
	writel_relaxed(temp, USB_PORTSC);
	while (cnt < PHY_RESUME_TIMEOUT_USEC) {
		if (!(readl_relaxed(USB_PORTSC) & PORTSC_PHCD) &&
			(readl_relaxed(USB_ULPI_VIEWPORT) & ULPI_SYNC_STATE))
			break;
		udelay(1);
		cnt++;
	}

	if (cnt >= PHY_RESUME_TIMEOUT_USEC) {
		/*
		 * This is a fatal error. Reset the link and
		 * PHY to make hsic working.
		 */
		dev_err(mehci->dev, "Unable to resume USB. Reset the hsic\n");
		msm_hsic_config_gpios(mehci, 0);
		msm_hsic_reset(mehci);
	}

skip_phy_resume:

	usb_hcd_resume_root_hub(hcd);

	atomic_set(&mehci->in_lpm, 0);

	if (atomic_read(&mehci->async_int)) {
		atomic_set(&mehci->async_int, 0);
		pm_runtime_put_noidle(mehci->dev);
		enable_irq(hcd->irq);
	}

	if (atomic_read(&mehci->pm_usage_cnt)) {
		atomic_set(&mehci->pm_usage_cnt, 0);
		pm_runtime_put_noidle(mehci->dev);
	}

	enable_irq(hcd->irq);
	dev_info(mehci->dev, "HSIC-USB exited from low power mode\n");

	if (pdata->consider_ipa_handshake) {
		dev_dbg(mehci->dev, "%s:Notify usb bam on resume complete\n",
			__func__);
		msm_bam_hsic_notify_on_resume();
	}

	return 0;
}
#endif

static void ehci_hsic_bus_vote_w(struct work_struct *w)
{
	struct msm_hsic_hcd *mehci =
			container_of(w, struct msm_hsic_hcd, bus_vote_w);
	int ret;

	ret = msm_bus_scale_client_update_request(mehci->bus_perf_client,
			mehci->bus_vote);
	if (ret)
		dev_err(mehci->dev, "%s: Failed to vote for bus bandwidth %d\n",
				__func__, ret);
}

static int msm_hsic_reset_done(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 __iomem *status_reg = &ehci->regs->port_status[0];
	int ret;

	ehci_writel(ehci, ehci_readl(ehci, status_reg) & ~(PORT_RWC_BITS |
					PORT_RESET), status_reg);

	ret = handshake(ehci, status_reg, PORT_RESET, 0, 1 * 1000);

	if (ret)
		pr_err("reset handshake failed in %s\n", __func__);
	else
		ehci_writel(ehci, ehci_readl(ehci, &ehci->regs->command) |
				CMD_RUN, &ehci->regs->command);

	return ret;
}

#define STS_GPTIMER0_INTERRUPT	BIT(24)
static irqreturn_t msm_hsic_irq(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	u32			status;
	int			ret;

	if (atomic_read(&mehci->in_lpm)) {
		dev_dbg(mehci->dev, "phy async intr\n");
		dbg_log_event(NULL, "Async IRQ", 0);
		ret = pm_runtime_get(mehci->dev);
		if ((ret == 1) || (ret == -EINPROGRESS)) {
			pm_runtime_put_noidle(mehci->dev);
		} else {
			disable_irq_nosync(hcd->irq);
			atomic_set(&mehci->async_int, 1);
		}

		return IRQ_HANDLED;
	}

	status = ehci_readl(ehci, &ehci->regs->status);

	if (status & STS_GPTIMER0_INTERRUPT) {
		int timeleft;

		dbg_log_event(NULL, "FPR: gpt0_isr", mehci->bus_reset);

		timeleft = GPT_CNT(ehci_readl(ehci,
						 &mehci->timer->gptimer1_ctrl));
		if (timeleft) {
			if (mehci->bus_reset) {
				ret = msm_hsic_reset_done(hcd);
				if (ret) {
					mehci->reset_again = 1;
					dbg_log_event(NULL, "RESET: fail", 0);
				}
			} else {
				ehci_writel(ehci, ehci_readl(ehci,
					&ehci->regs->command) | CMD_RUN,
					&ehci->regs->command);
			}
		} else {
			if (mehci->bus_reset)
				mehci->reset_again = 1;
			else
				mehci->resume_again = 1;
		}

		dbg_log_event(NULL, "FPR: timeleft", timeleft);

		complete(&mehci->gpt0_completion);
		ehci_writel(ehci, STS_GPTIMER0_INTERRUPT, &ehci->regs->status);
	}

	return ehci_irq(hcd);
}

static int ehci_hsic_reset(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;
	int retval;
	u32 temp;

	mehci->timer = USB_HS_GPTIMER_BASE;
	ehci->caps = USB_CAPLENGTH;
	hcd->has_tt = 1;

	retval = ehci_setup(hcd);
	if (retval)
		return retval;

	/* bursts of unspecified length. */
	writel_relaxed(0, USB_AHBBURST);

	/* Use the AHB transactor and configure async bridge bypass */
#define MSM_USB_ASYNC_BRIDGE_BYPASS BIT(31)
	if (pdata->ahb_async_bridge_bypass)
		writel_relaxed(0x08 | MSM_USB_ASYNC_BRIDGE_BYPASS, USB_AHBMODE);
	else
		writel_relaxed(0x08, USB_AHBMODE);

	if (pdata->dis_internal_clk_gating) {
		temp = readl_relaxed(USB_GENCONFIG2);
		temp &= ~GENCFG2_SYS_CLK_HOST_DEV_GATE_EN;
		writel_relaxed(temp, USB_GENCONFIG2);
	}

	/* Disable streaming mode and select host mode */
	writel_relaxed(0x13, USB_USBMODE);

	return 0;
}

#ifdef CONFIG_PM

#define RESET_RETRY_LIMIT 3
#define RESET_SIGNAL_TIME_SOF_USEC (50 * 1000)
#define RESET_SIGNAL_TIME_USEC (20 * 1000)
static void ehci_hsic_reset_sof_bug_handler(struct usb_hcd *hcd, u32 val)
{
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;
	u32 __iomem *status_reg = &ehci->regs->port_status[0];
	u32 cmd;
	unsigned long flags;
	int retries = 0, ret, cnt = RESET_SIGNAL_TIME_USEC;
	s32 next_latency = 0;

	if (pdata && pdata->swfi_latency) {
		next_latency = pdata->swfi_latency + 1;
		pm_qos_update_request(&mehci->pm_qos_req_dma, next_latency);
		if (pdata->standalone_latency)
			next_latency = pdata->standalone_latency + 1;
		else
			next_latency = PM_QOS_DEFAULT_VALUE;
	}

	mehci->bus_reset = 1;

	/* Halt the controller */
	cmd = ehci_readl(ehci, &ehci->regs->command);
	cmd &= ~CMD_RUN;
	ehci_writel(ehci, cmd, &ehci->regs->command);
	ret = handshake(ehci, &ehci->regs->status, STS_HALT,
			STS_HALT, 16 * 125);
	if (ret) {
		pr_err("halt handshake fatal error\n");
		dbg_log_event(NULL, "HALT: fatal", 0);
		goto fail;
	}

retry:
	retries++;
	dbg_log_event(NULL, "RESET: start", retries);
	pr_debug("reset begin %d\n", retries);
	mehci->reset_again = 0;
	spin_lock_irqsave(&ehci->lock, flags);
	ehci_writel(ehci, val, status_reg);
	ehci_writel(ehci, GPT_LD(RESET_SIGNAL_TIME_USEC - 1),
					&mehci->timer->gptimer0_ld);
	ehci_writel(ehci, GPT_RESET | GPT_RUN,
			&mehci->timer->gptimer0_ctrl);
	ehci_writel(ehci, INTR_MASK | STS_GPTIMER0_INTERRUPT,
			&ehci->regs->intr_enable);

	ehci_writel(ehci, GPT_LD(RESET_SIGNAL_TIME_SOF_USEC - 1),
			&mehci->timer->gptimer1_ld);
	ehci_writel(ehci, GPT_RESET | GPT_RUN,
		&mehci->timer->gptimer1_ctrl);

	spin_unlock_irqrestore(&ehci->lock, flags);
	wait_for_completion(&mehci->gpt0_completion);

	if (!mehci->reset_again)
		goto done;

	if (handshake(ehci, status_reg, PORT_RESET, 0, 10 * 1000)) {
		pr_err("reset handshake fatal error\n");
		dbg_log_event(NULL, "RESET: fatal", retries);
		goto fail;
	}

	if (retries < RESET_RETRY_LIMIT)
		goto retry;

	/* complete reset in tight loop */
	pr_info("RESET in tight loop\n");
	dbg_log_event(NULL, "RESET: tight", 0);

	spin_lock_irqsave(&ehci->lock, flags);
	ehci_writel(ehci, val, status_reg);
	while (cnt--)
		udelay(1);
	ret = msm_hsic_reset_done(hcd);
	spin_unlock_irqrestore(&ehci->lock, flags);
	if (ret) {
		pr_err("RESET in tight loop failed\n");
		dbg_log_event(NULL, "RESET: tight failed", 0);
		goto fail;
	}

done:
	dbg_log_event(NULL, "RESET: done", retries);
	pr_debug("reset completed\n");
fail:
	mehci->bus_reset = 0;
	if (next_latency)
		pm_qos_update_request(&mehci->pm_qos_req_dma, next_latency);
}

static int ehci_hsic_bus_suspend(struct usb_hcd *hcd)
{
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	if (!(readl_relaxed(USB_PORTSC) & PORT_PE)) {
		dbg_log_event(NULL, "RH suspend attempt failed", 0);
		dev_dbg(mehci->dev, "%s:port is not enabled skip suspend\n",
				__func__);
		return -EAGAIN;
	}

	dbg_log_event(NULL, "Suspend RH", 0);
	return ehci_bus_suspend(hcd);
}

#define RESUME_RETRY_LIMIT		3
#define RESUME_SIGNAL_TIME_USEC		(21 * 1000)
#define RESUME_SIGNAL_TIME_SOF_USEC	(23 * 1000)
static int msm_hsic_resume_thread(void *data)
{
	struct msm_hsic_hcd *mehci = data;
	struct usb_hcd *hcd = hsic_to_hcd(mehci);
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	u32			temp;
	unsigned long		resume_needed = 0;
	int			retry_cnt = 0;
	int			tight_resume = 0;
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;
	s32 next_latency = 0;

	dbg_log_event(NULL, "Resume RH", 0);

	if (pdata && pdata->swfi_latency) {
		next_latency = pdata->swfi_latency + 1;
		pm_qos_update_request(&mehci->pm_qos_req_dma, next_latency);
		if (pdata->standalone_latency)
			next_latency = pdata->standalone_latency + 1;
		else
			next_latency = PM_QOS_DEFAULT_VALUE;
	}

	/* keep delay between bus states */
	if (time_before_eq(jiffies, ehci->next_statechange))
		usleep_range(10000, 10000);

	spin_lock_irq(&ehci->lock);
	if (!HCD_HW_ACCESSIBLE(hcd)) {
		mehci->resume_status = -ESHUTDOWN;
		goto exit;
	}

	if (unlikely(ehci->debug)) {
		if (!dbgp_reset_prep(hcd))
			ehci->debug = NULL;
		else
			dbgp_external_startup(hcd);
	}

	/* at least some APM implementations will try to deliver
	 * IRQs right away, so delay them until we're ready.
	 */
	ehci_writel(ehci, 0, &ehci->regs->intr_enable);

	/* re-init operational registers */
	ehci_writel(ehci, 0, &ehci->regs->segment);
	ehci_writel(ehci, ehci->periodic_dma, &ehci->regs->frame_list);
	ehci_writel(ehci, (u32) ehci->async->qh_dma, &ehci->regs->async_next);

	/*CMD_RUN will be set after, PORT_RESUME gets cleared*/
	if (ehci->resume_sof_bug)
		ehci->command &= ~CMD_RUN;

	/* restore CMD_RUN, framelist size, and irq threshold */
	ehci_writel(ehci, ehci->command, &ehci->regs->command);

	/* manually resume the ports we suspended during bus_suspend() */
resume_again:
	if (retry_cnt >= RESUME_RETRY_LIMIT) {
		pr_info("retry count(%d) reached max, resume in tight loop\n",
					retry_cnt);
		tight_resume = 1;
	}


	temp = ehci_readl(ehci, &ehci->regs->port_status[0]);
	temp &= ~(PORT_RWC_BITS | PORT_WAKE_BITS);
	if (test_bit(0, &ehci->bus_suspended) && (temp & PORT_SUSPEND)) {
		temp |= PORT_RESUME;
		set_bit(0, &resume_needed);
	}
	dbg_log_event(NULL, "FPR: Set", temp);
	ehci_writel(ehci, temp, &ehci->regs->port_status[0]);

	/* HSIC controller has a h/w bug due to which it can try to send SOFs
	 * (start of frames) during port resume resulting in phy lockup. HSIC hw
	 * controller in MSM clears FPR bit after driving the resume signal for
	 * 20ms. Workaround is to stop SOFs before driving resume and then start
	 * sending SOFs immediately. Need to send SOFs within 3ms of resume
	 * completion otherwise peripheral may enter undefined state. As
	 * usleep_range does not gurantee exact sleep time, GPTimer is used to
	 * to time the resume sequence. If driver exceeds allowable time SOFs,
	 * repeat the resume process.
	 */
	if (ehci->resume_sof_bug && resume_needed) {
		if (!tight_resume) {
			mehci->resume_again = 0;
			ehci_writel(ehci, GPT_LD(RESUME_SIGNAL_TIME_USEC - 1),
					&mehci->timer->gptimer0_ld);
			ehci_writel(ehci, GPT_RESET | GPT_RUN,
					&mehci->timer->gptimer0_ctrl);
			ehci_writel(ehci, INTR_MASK | STS_GPTIMER0_INTERRUPT,
					&ehci->regs->intr_enable);

			ehci_writel(ehci, GPT_LD(
					RESUME_SIGNAL_TIME_SOF_USEC - 1),
					&mehci->timer->gptimer1_ld);
			ehci_writel(ehci, GPT_RESET | GPT_RUN,
				&mehci->timer->gptimer1_ctrl);

			spin_unlock_irq(&ehci->lock);
			wait_for_completion(&mehci->gpt0_completion);
			spin_lock_irq(&ehci->lock);
		} else {
			dbg_log_event(NULL, "FPR: Tightloop", 0);
			/* do the resume in a tight loop */
			handshake(ehci, &ehci->regs->port_status[0],
				PORT_RESUME, 0, 22 * 1000);
			ehci_writel(ehci, ehci_readl(ehci,
				&ehci->regs->command) | CMD_RUN,
				&ehci->regs->command);
		}

		if (mehci->resume_again) {
			int temp;

			dbg_log_event(NULL, "FPR: Re-Resume", retry_cnt);
			pr_info("FPR: retry count: %d\n", retry_cnt);
			spin_unlock_irq(&ehci->lock);
			temp = ehci_readl(ehci, &ehci->regs->port_status[0]);
			temp &= ~PORT_RWC_BITS;
			temp |= PORT_SUSPEND;
			ehci_writel(ehci, temp, &ehci->regs->port_status[0]);
			/* Keep the bus idle for 5ms so that peripheral
			 * can detect and initiate suspend
			 */
			usleep_range(5000, 5000);
			dbg_log_event(NULL,
				"FPR: RResume",
				ehci_readl(ehci, &ehci->regs->port_status[0]));
			spin_lock_irq(&ehci->lock);
			mehci->resume_again = 0;
			retry_cnt++;
			goto resume_again;
		}
	}

	dbg_log_event(NULL, "FPR: RT-Done", 0);
	mehci->resume_status = 1;
exit:
	spin_unlock_irq(&ehci->lock);
	complete(&mehci->rt_completion);
	if (next_latency)
		pm_qos_update_request(&mehci->pm_qos_req_dma, next_latency);

	return 0;
}

static int ehci_hsic_bus_resume(struct usb_hcd *hcd)
{
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct task_struct	*resume_thread = NULL;
	struct msm_hsic_host_platform_data *pdata =  mehci->dev->platform_data;

	if (pdata->resume_gpio)
		gpio_direction_output(pdata->resume_gpio, 1);

	if (!mehci->ehci.resume_sof_bug) {
		ehci_bus_resume(hcd);
	} else {
		mehci->resume_status = 0;
		resume_thread = kthread_run(msm_hsic_resume_thread,
				mehci, "hsic_resume_thread");
		if (IS_ERR(resume_thread)) {
			pr_err("Error creating resume thread:%lu\n",
					PTR_ERR(resume_thread));
			return PTR_ERR(resume_thread);
		}

		wait_for_completion(&mehci->rt_completion);

		if (mehci->resume_status < 0)
			return mehci->resume_status;

		dbg_log_event(NULL, "FPR: Wokeup", 0);
		spin_lock_irq(&ehci->lock);

		ehci->next_statechange = jiffies + msecs_to_jiffies(5);
		hcd->state = HC_STATE_RUNNING;
		ehci->rh_state = EHCI_RH_RUNNING;
		ehci->command |= CMD_RUN;

		/* Now we can safely re-enable irqs */
		ehci_writel(ehci, INTR_MASK, &ehci->regs->intr_enable);
		(void) ehci_readl(ehci, &ehci->regs->intr_enable);

		spin_unlock_irq(&ehci->lock);
	}

	if (pdata->resume_gpio)
		gpio_direction_output(pdata->resume_gpio, 0);

	return 0;
}
#else

#define ehci_hsic_bus_suspend	NULL
#define ehci_hsic_bus_resume	NULL

#endif	/* CONFIG_PM */

static void ehci_msm_set_autosuspend_delay(struct usb_device *dev)
{
	if (!dev->parent) /*for root hub no delay*/
		pm_runtime_set_autosuspend_delay(&dev->dev, 0);
	else
		pm_runtime_set_autosuspend_delay(&dev->dev, 200);
}

static int ehci_msm_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
				gfp_t mem_flags)
{
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct usb_host_bam_type *usb_host_bam =
			(struct usb_host_bam_type *)urb->priv_data;

	if (usb_host_bam && mehci && mehci->enable_hbm)
		return hbm_urb_enqueue(hcd, urb, mem_flags);
	return ehci_urb_enqueue(hcd, urb, mem_flags);
}

static struct hc_driver msm_hsic_driver = {
	.description		= hcd_name,
	.product_desc		= "Qualcomm EHCI Host Controller using HSIC",
	.hcd_priv_size		= sizeof(struct msm_hsic_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= msm_hsic_irq,
	.flags			= HCD_USB2 | HCD_MEMORY | HCD_OLD_ENUM,

	.reset			= ehci_hsic_reset,
	.start			= ehci_run,

	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_msm_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,
	.clear_tt_buffer_complete	 = ehci_clear_tt_buffer_complete,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	/*
	 * PM support
	 */
	.bus_suspend		= ehci_hsic_bus_suspend,
	.bus_resume		= ehci_hsic_bus_resume,

	.log_urb		= dbg_log_event,
	.dump_regs		= dump_hsic_regs,

	.set_autosuspend_delay = ehci_msm_set_autosuspend_delay,
	.reset_sof_bug_handler	= ehci_hsic_reset_sof_bug_handler,
};

static int msm_hsic_init_clocks(struct msm_hsic_hcd *mehci, u32 init)
{
	int ret = 0;

	if (!init)
		goto put_clocks;

	/*core_clk is required for LINK protocol engine
	 *clock rate appropriately set by target specific clock driver */
	mehci->core_clk = clk_get(mehci->dev, "core_clk");
	if (IS_ERR(mehci->core_clk)) {
		ret = PTR_ERR(mehci->core_clk);
		mehci->core_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(mehci->dev, "failed to get core_clk\n");
		return ret;
	}

	/* alt_core_clk is for LINK to be used during PHY RESET in
	 * targets on which link does NOT use asynchronous reset methodology.
	 * clock rate appropriately set by target specific clock driver */
	mehci->alt_core_clk = clk_get(mehci->dev, "alt_core_clk");
	if (IS_ERR(mehci->alt_core_clk)) {
		ret = PTR_ERR(mehci->alt_core_clk);
		mehci->alt_core_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_dbg(mehci->dev, "failed to get alt_core_clk\n");
		else
			goto put_core_clk;
	}

	/* phy_clk is required for HSIC PHY operation
	 * clock rate appropriately set by target specific clock driver */
	mehci->phy_clk = clk_get(mehci->dev, "phy_clk");
	if (IS_ERR(mehci->phy_clk)) {
		ret = PTR_ERR(mehci->phy_clk);
		mehci->phy_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(mehci->dev, "failed to get phy_clk\n");
		goto put_alt_core_clk;
	}

	/* 10MHz cal_clk is required for calibration of I/O pads */
	mehci->cal_clk = clk_get(mehci->dev, "cal_clk");
	if (IS_ERR(mehci->cal_clk)) {
		ret = PTR_ERR(mehci->cal_clk);
		mehci->cal_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(mehci->dev, "failed to get cal_clk\n");
		goto put_phy_clk;
	}

	/* ahb_clk is required for data transfers */
	mehci->ahb_clk = clk_get(mehci->dev, "iface_clk");
	if (IS_ERR(mehci->ahb_clk)) {
		ret = PTR_ERR(mehci->ahb_clk);
		mehci->ahb_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(mehci->dev, "failed to get iface_clk\n");
		goto put_cal_clk;
	}

	/*
	 * Inactivity_clk is required for hsic bam inactivity timer.
	 * This clock is not compulsory and is defined in clock lookup
	 * only for targets that need to use the inactivity timer feature.
	 */
	mehci->inactivity_clk = clk_get(mehci->dev, "inactivity_clk");
	if (IS_ERR(mehci->inactivity_clk))
		dev_dbg(mehci->dev, "failed to get inactivity_clk\n");

	clk_prepare_enable(mehci->core_clk);
	clk_prepare_enable(mehci->phy_clk);
	clk_prepare_enable(mehci->cal_clk);
	clk_prepare_enable(mehci->ahb_clk);
	if (!IS_ERR(mehci->inactivity_clk))
		clk_prepare_enable(mehci->inactivity_clk);

	return 0;

put_clocks:
	if (!atomic_read(&mehci->in_lpm)) {
		clk_disable_unprepare(mehci->core_clk);
		clk_disable_unprepare(mehci->phy_clk);
		clk_disable_unprepare(mehci->cal_clk);
		clk_disable_unprepare(mehci->ahb_clk);
		if (!IS_ERR(mehci->inactivity_clk))
			clk_disable_unprepare(mehci->inactivity_clk);
	}
	if (!IS_ERR(mehci->inactivity_clk))
		clk_put(mehci->inactivity_clk);
	clk_put(mehci->ahb_clk);
put_cal_clk:
	clk_put(mehci->cal_clk);
put_phy_clk:
	clk_put(mehci->phy_clk);
put_alt_core_clk:
	if (mehci->alt_core_clk)
		clk_put(mehci->alt_core_clk);
put_core_clk:
	clk_put(mehci->core_clk);

	return ret;
}
static irqreturn_t hsic_peripheral_status_change(int irq, void *dev_id)
{
	struct msm_hsic_hcd *mehci = dev_id;

	pr_debug("%s: mehci:%p dev_id:%p\n", __func__, mehci, dev_id);

	if (mehci)
		msm_hsic_config_gpios(mehci, 0);

	return IRQ_HANDLED;
}

static irqreturn_t msm_hsic_wakeup_irq(int irq, void *data)
{
	struct msm_hsic_hcd *mehci = data;
	int ret;

	if (irq == mehci->async_irq) {
		mehci->async_int_cnt++;
		dbg_log_event(NULL, "Remote Wakeup (ASYNC) IRQ",
							 mehci->async_int_cnt);
	} else {
		mehci->wakeup_int_cnt++;
		dbg_log_event(NULL, "Remote Wakeup IRQ", mehci->wakeup_int_cnt);
	}
	dev_dbg(mehci->dev, "%s: hsic remote wakeup interrupt %d cnt: %u, %u\n",
		    __func__, irq, mehci->wakeup_int_cnt, mehci->async_int_cnt);

	wake_lock(&mehci->wlock);

	if (mehci->wakeup_irq) {
		spin_lock(&mehci->wakeup_lock);
		if (mehci->wakeup_irq_enabled) {
			mehci->wakeup_irq_enabled = 0;
			disable_irq_wake(irq);
			disable_irq_nosync(irq);
		}
		spin_unlock(&mehci->wakeup_lock);
	}

	if (!atomic_read(&mehci->pm_usage_cnt)) {
		ret = pm_runtime_get(mehci->dev);
		/*
		 * HSIC runtime resume can race with us.
		 * if we are active (ret == 1) or resuming
		 * (ret == -EINPROGRESS), decrement the
		 * PM usage counter before returning.
		 */
		if ((ret == 1) || (ret == -EINPROGRESS))
			pm_runtime_put_noidle(mehci->dev);
		else
			atomic_set(&mehci->pm_usage_cnt, 1);
	}

	return IRQ_HANDLED;
}

static int ehci_hsic_msm_bus_show(struct seq_file *s, void *unused)
{
	if (debug_bus_voting_enabled)
		seq_printf(s, "enabled\n");
	else
		seq_printf(s, "disabled\n");

	return 0;
}

static int ehci_hsic_msm_bus_open(struct inode *inode, struct file *file)
{
	return single_open(file, ehci_hsic_msm_bus_show, inode->i_private);
}

static ssize_t ehci_hsic_msm_bus_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[8];
	int ret;
	struct seq_file *s = file->private_data;
	struct msm_hsic_hcd *mehci = s->private;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "enable", 6)) {
		/* Do not vote here. Let hsic driver decide when to vote */
		debug_bus_voting_enabled = true;
	} else {
		debug_bus_voting_enabled = false;
		if (mehci->bus_perf_client) {
			ret = msm_bus_scale_client_update_request(
					mehci->bus_perf_client, 0);
			if (ret)
				dev_err(mehci->dev, "%s: Failed to devote "
					   "for bus bw %d\n", __func__, ret);
		}
	}

	return count;
}

const struct file_operations ehci_hsic_msm_bus_fops = {
	.open = ehci_hsic_msm_bus_open,
	.read = seq_read,
	.write = ehci_hsic_msm_bus_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ehci_hsic_msm_wakeup_cnt_show(struct seq_file *s, void *unused)
{
	struct msm_hsic_hcd *mehci = s->private;

	seq_printf(s, "%u\n", mehci->wakeup_int_cnt);

	return 0;
}

static int ehci_hsic_msm_wakeup_cnt_open(struct inode *inode, struct file *f)
{
	return single_open(f, ehci_hsic_msm_wakeup_cnt_show, inode->i_private);
}

const struct file_operations ehci_hsic_msm_wakeup_cnt_fops = {
	.open = ehci_hsic_msm_wakeup_cnt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ehci_hsic_msm_data_events_show(struct seq_file *s, void *unused)
{
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&dbg_hsic_data.lck, flags);

	i = dbg_hsic_data.idx;
	for (dbg_inc(&i); i != dbg_hsic_data.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_hsic_data.buf[i], DBG_MSG_LEN))
			continue;
		seq_printf(s, "%s\n", dbg_hsic_data.buf[i]);
	}

	read_unlock_irqrestore(&dbg_hsic_data.lck, flags);

	return 0;
}

static int ehci_hsic_msm_data_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, ehci_hsic_msm_data_events_show, inode->i_private);
}

const struct file_operations ehci_hsic_msm_dbg_data_fops = {
	.open = ehci_hsic_msm_data_events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ehci_hsic_msm_ctrl_events_show(struct seq_file *s, void *unused)
{
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&dbg_hsic_ctrl.lck, flags);

	i = dbg_hsic_ctrl.idx;
	for (dbg_inc(&i); i != dbg_hsic_ctrl.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_hsic_ctrl.buf[i], DBG_MSG_LEN))
			continue;
		seq_printf(s, "%s\n", dbg_hsic_ctrl.buf[i]);
	}

	read_unlock_irqrestore(&dbg_hsic_ctrl.lck, flags);

	return 0;
}

static int ehci_hsic_msm_ctrl_events_open(struct inode *inode, struct file *f)
{
	return single_open(f, ehci_hsic_msm_ctrl_events_show, inode->i_private);
}

const struct file_operations ehci_hsic_msm_dbg_ctrl_fops = {
	.open = ehci_hsic_msm_ctrl_events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *ehci_hsic_msm_dbg_root;
static int ehci_hsic_msm_debugfs_init(struct msm_hsic_hcd *mehci)
{
	struct dentry *ehci_hsic_msm_dentry;

	ehci_hsic_msm_dbg_root = debugfs_create_dir("ehci_hsic_msm_dbg", NULL);

	if (!ehci_hsic_msm_dbg_root || IS_ERR(ehci_hsic_msm_dbg_root))
		return -ENODEV;

	ehci_hsic_msm_dentry = debugfs_create_file("bus_voting",
		S_IRUGO | S_IWUSR,
		ehci_hsic_msm_dbg_root, mehci,
		&ehci_hsic_msm_bus_fops);

	if (!ehci_hsic_msm_dentry) {
		debugfs_remove_recursive(ehci_hsic_msm_dbg_root);
		return -ENODEV;
	}

	ehci_hsic_msm_dentry = debugfs_create_file("wakeup_cnt",
		S_IRUGO,
		ehci_hsic_msm_dbg_root, mehci,
		&ehci_hsic_msm_wakeup_cnt_fops);

	if (!ehci_hsic_msm_dentry) {
		debugfs_remove_recursive(ehci_hsic_msm_dbg_root);
		return -ENODEV;
	}

	ehci_hsic_msm_dentry = debugfs_create_file("show_ctrl_events",
		S_IRUGO,
		ehci_hsic_msm_dbg_root, mehci,
		&ehci_hsic_msm_dbg_ctrl_fops);

	if (!ehci_hsic_msm_dentry) {
		debugfs_remove_recursive(ehci_hsic_msm_dbg_root);
		return -ENODEV;
	}

	ehci_hsic_msm_dentry = debugfs_create_file("show_data_events",
		S_IRUGO,
		ehci_hsic_msm_dbg_root, mehci,
		&ehci_hsic_msm_dbg_data_fops);

	if (!ehci_hsic_msm_dentry) {
		debugfs_remove_recursive(ehci_hsic_msm_dbg_root);
		return -ENODEV;
	}

	return 0;
}

static void ehci_hsic_msm_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ehci_hsic_msm_dbg_root);
}

struct msm_hsic_host_platform_data *msm_hsic_dt_to_pdata(
				struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct msm_hsic_host_platform_data *pdata;
	int res_gpio;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "unable to allocate platform data\n");
		return NULL;
	}

	res_gpio = of_get_named_gpio(node, "hsic,strobe-gpio", 0);
	if (res_gpio < 0)
		res_gpio = 0;
	pdata->strobe = res_gpio;

	res_gpio = of_get_named_gpio(node, "hsic,data-gpio", 0);
	if (res_gpio < 0)
		res_gpio = 0;
	pdata->data = res_gpio;

	res_gpio = of_get_named_gpio(node, "hsic,resume-gpio", 0);
	if (res_gpio < 0)
		res_gpio = 0;
	pdata->resume_gpio = res_gpio;

	pdata->phy_sof_workaround = of_property_read_bool(node,
					"qcom,phy-sof-workaround");
	pdata->dis_internal_clk_gating = of_property_read_bool(node,
					"qcom,disable-internal-clk-gating");
	pdata->phy_susp_sof_workaround = of_property_read_bool(node,
					"qcom,phy-susp-sof-workaround");
	pdata->ignore_cal_pad_config = of_property_read_bool(node,
					"hsic,ignore-cal-pad-config");
	of_property_read_u32(node, "hsic,strobe-pad-offset",
					&pdata->strobe_pad_offset);
	of_property_read_u32(node, "hsic,data-pad-offset",
					&pdata->data_pad_offset);
	of_property_read_u32(node, "hsic,reset-delay",
					&pdata->reset_delay);
	of_property_read_u32(node, "hsic,log2-itc",
					&pdata->log2_irq_thresh);
	if (pdata->log2_irq_thresh > 6)
		pdata->log2_irq_thresh = 0;

	pdata->bus_scale_table = msm_bus_cl_get_pdata(pdev);

	pdata->pool_64_bit_align = of_property_read_bool(node,
				"qcom,pool-64-bit-align");
	pdata->enable_hbm = of_property_read_bool(node,
				"qcom,enable-hbm");
	pdata->disable_park_mode = (of_property_read_bool(node,
				"qcom,disable-park-mode"));
	pdata->consider_ipa_handshake = (of_property_read_bool(node,
				"hsic,consider-ipa-handshake"));
	pdata->ahb_async_bridge_bypass = of_property_read_bool(node,
				"qcom,ahb-async-bridge-bypass");
	pdata->disable_cerr = of_property_read_bool(node,
				"hsic,disable-cerr");

	return pdata;
}


static int ehci_hsic_msm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	struct msm_hsic_hcd *mehci;
	struct msm_hsic_host_platform_data *pdata;
	unsigned long wakeup_irq_flags = 0;
	int ret;

	dev_dbg(&pdev->dev, "ehci_msm-hsic probe\n");

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdev->dev.platform_data = msm_hsic_dt_to_pdata(pdev);
		dev_set_name(&pdev->dev, ehci_msm_hsic_driver.driver.name);
	} else {
		/* explicitly pass wakeup_irq flag for !DT */
		wakeup_irq_flags = IRQF_TRIGGER_HIGH;
	}
	if (!pdev->dev.platform_data)
		dev_dbg(&pdev->dev, "No platform data given\n");

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &ehci_msm_hsic_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	/* After parent device's probe is executed, it will be put in suspend
	 * mode. When child device's probe is called, driver core is not
	 * resuming parent device due to which parent will be in suspend even
	 * though child is active. Hence resume the parent device explicitly.
	 */
	if (pdev->dev.parent)
		pm_runtime_get_sync(pdev->dev.parent);

	hcd = usb_create_hcd(&msm_hsic_driver, &pdev->dev,
				dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		ret = -ENOMEM;
		goto put_parent;
	}

	hcd_to_bus(hcd)->skip_resume = true;

	hcd->irq = platform_get_irq(pdev, 0);
	if (hcd->irq < 0) {
		dev_err(&pdev->dev, "Unable to get IRQ resource\n");
		ret = hcd->irq;
		goto put_hcd;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get memory resource\n");
		ret = -ENODEV;
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto put_hcd;
	}

	mehci = hcd_to_hsic(hcd);
	mehci->dev = &pdev->dev;
	pdata = mehci->dev->platform_data;
	platform_set_drvdata(pdev, hcd);

	spin_lock_init(&mehci->wakeup_lock);

	if (pdata->phy_sof_workaround) {
		/* Enable ALL workarounds related to PHY SOF bugs */
		mehci->ehci.susp_sof_bug = 1;
		mehci->ehci.reset_sof_bug = 1;
		mehci->ehci.resume_sof_bug = 1;
	} else if (pdata->phy_susp_sof_workaround) {
		/* Only SUSP SOF hardware bug exists, rest all not present */
		mehci->ehci.susp_sof_bug = 1;
	}

	if (pdata->reset_delay)
		mehci->ehci.reset_delay = pdata->reset_delay;

	mehci->ehci.pool_64_bit_align = pdata->pool_64_bit_align;
	mehci->enable_hbm = pdata->enable_hbm;

	if (pdata) {
		mehci->ehci.log2_irq_thresh = pdata->log2_irq_thresh;
		mehci->ehci.disable_cerr = pdata->disable_cerr;
	}

	ret = msm_hsic_init_gdsc(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize GDSC\n");
		ret = -ENODEV;
		goto put_hcd;
	}

	res = platform_get_resource_byname(pdev,
			IORESOURCE_IRQ,
			"peripheral_status_irq");
	if (res)
		mehci->peripheral_status_irq = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "wakeup");
	if (res) {
		mehci->wakeup_irq = res->start;
		dev_dbg(mehci->dev, "wakeup_irq: %d\n", mehci->wakeup_irq);
	}

	ret = msm_hsic_init_clocks(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize clocks\n");
		goto unmap;
	}

	ret = msm_hsic_init_vddcx(mehci, 1);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize VDDCX\n");
		ret = -ENODEV;
		goto deinit_clocks;
	}

	init_completion(&mehci->rt_completion);
	init_completion(&mehci->gpt0_completion);

	msm_hsic_phy_reset(mehci);

	ehci_wq = create_singlethread_workqueue("ehci_wq");
	if (!ehci_wq) {
		dev_err(&pdev->dev, "unable to create workqueue\n");
		ret = -ENOMEM;
		goto deinit_vddcx;
	}

	INIT_WORK(&mehci->bus_vote_w, ehci_hsic_bus_vote_w);

	ret = usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
	if (ret) {
		dev_err(&pdev->dev, "unable to register HCD\n");
		goto destroy_wq;
	}

	ret = msm_hsic_start(mehci);
	if (ret) {
		dev_err(&pdev->dev, "unable to initialize PHY\n");
		goto destroy_wq;
	}

	device_init_wakeup(&pdev->dev, 1);
	wake_lock_init(&mehci->wlock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	wake_lock(&mehci->wlock);

	if (mehci->peripheral_status_irq) {
		ret = request_threaded_irq(mehci->peripheral_status_irq,
			NULL, hsic_peripheral_status_change,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
						| IRQF_SHARED,
			"hsic_peripheral_status", mehci);
		if (ret)
			dev_err(&pdev->dev, "%s:request_irq:%d failed:%d",
				__func__, mehci->peripheral_status_irq, ret);
	}

	/* configure wakeup irq */
	if (mehci->wakeup_irq) {
		/* In case if wakeup gpio is pulled high at this point
		 * remote wakeup interrupt fires right after request_irq.
		 * Remote wake up interrupt only needs to be enabled when
		 * HSIC bus goes to suspend.
		 */
		irq_set_status_flags(mehci->wakeup_irq, IRQ_NOAUTOEN);
		ret = request_irq(mehci->wakeup_irq, msm_hsic_wakeup_irq,
				wakeup_irq_flags,
				"msm_hsic_wakeup", mehci);
		if (ret) {
			dev_err(&pdev->dev, "request_irq(%d) failed: %d\n",
					mehci->wakeup_irq, ret);
			mehci->wakeup_irq = 0;
		}
	}

	mehci->async_irq = platform_get_irq_byname(pdev, "async_irq");
	if (mehci->async_irq < 0) {
		dev_dbg(&pdev->dev, "platform_get_irq for async_int failed\n");
		mehci->async_irq = 0;
	} else {
		ret = request_irq(mehci->async_irq, msm_hsic_wakeup_irq,
				IRQF_TRIGGER_RISING, "msm_hsic_async", mehci);
		if (ret) {
			dev_err(&pdev->dev, "request irq failed (ASYNC INT)\n");
			mehci->async_irq = 0;
		} else if (!mehci->wakeup_irq) {
			/* Async IRQ is used only in absence of dedicated irq */
			enable_irq_wake(mehci->async_irq);
		}
	}

	ret = ehci_hsic_msm_debugfs_init(mehci);
	if (ret)
		dev_dbg(&pdev->dev, "mode debugfs file is"
			"not available\n");

	if (pdata && pdata->bus_scale_table) {
		mehci->bus_perf_client =
		    msm_bus_scale_register_client(pdata->bus_scale_table);
		/* Configure BUS performance parameters for MAX bandwidth */
		if (mehci->bus_perf_client) {
			mehci->bus_vote = true;
			queue_work(ehci_wq, &mehci->bus_vote_w);
		} else {
			dev_err(&pdev->dev, "%s: Failed to register BUS "
						"scaling client!!\n", __func__);
		}
	}

	__mehci = mehci;

	if (pdata && pdata->standalone_latency)
		pm_qos_add_request(&mehci->pm_qos_req_dma,
			PM_QOS_CPU_DMA_LATENCY, pdata->standalone_latency + 1);

	/*
	 * This pdev->dev is assigned parent of root-hub by USB core,
	 * hence, runtime framework automatically calls this driver's
	 * runtime APIs based on root-hub's state.
	 */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	/* Decrement the parent device's counter after probe.
	 * As child is active, parent will not be put into
	 * suspend mode.
	 */
	if (pdev->dev.parent)
		pm_runtime_put_sync(pdev->dev.parent);

	if (mehci->enable_hbm)
		hbm_init(hcd, pdata->disable_park_mode);

	if (pdata && pdata->consider_ipa_handshake)
		msm_bam_set_hsic_host_dev(&pdev->dev);

	return 0;

destroy_wq:
	destroy_workqueue(ehci_wq);
deinit_vddcx:
	msm_hsic_init_vddcx(mehci, 0);
	msm_hsic_init_gdsc(mehci, 0);
deinit_clocks:
	msm_hsic_init_clocks(mehci, 0);
unmap:
	iounmap(hcd->regs);
put_hcd:
	usb_put_hcd(hcd);
put_parent:
	if (pdev->dev.parent)
		pm_runtime_put_sync(pdev->dev.parent);

	return ret;
}

static int ehci_hsic_msm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);
	struct msm_hsic_host_platform_data *pdata = mehci->dev->platform_data;

	if (pdata && pdata->consider_ipa_handshake)
		msm_bam_set_hsic_host_dev(NULL);

	/* If the device was removed no need to call pm_runtime_disable */
	if (pdev->dev.power.power_state.event != PM_EVENT_INVALID)
		pm_runtime_disable(&pdev->dev);

	pm_runtime_set_suspended(&pdev->dev);

	if (mehci->enable_hbm)
		hbm_uninit();

	/* Remove the HCD prior to releasing our resources. */
	usb_remove_hcd(hcd);

	if (pdata && pdata->standalone_latency)
		pm_qos_remove_request(&mehci->pm_qos_req_dma);

	if (mehci->peripheral_status_irq)
		free_irq(mehci->peripheral_status_irq, mehci);

	if (mehci->wakeup_irq) {
		if (mehci->wakeup_irq_enabled)
			disable_irq_wake(mehci->wakeup_irq);
		free_irq(mehci->wakeup_irq, mehci);
	}

	if (mehci->async_irq) {
		/* Async IRQ is used only in absence of dedicated wakeup irq */
		if (!mehci->wakeup_irq)
			disable_irq_wake(mehci->async_irq);
		free_irq(mehci->async_irq, mehci);
	}
	/*
	 * If the update request is called after unregister, the request will
	 * fail. Results are undefined if unregister is called in the middle of
	 * update request.
	 */
	mehci->bus_vote = false;
	cancel_work_sync(&mehci->bus_vote_w);

	if (mehci->bus_perf_client)
		msm_bus_scale_unregister_client(mehci->bus_perf_client);

	ehci_hsic_msm_debugfs_cleanup();
	device_init_wakeup(&pdev->dev, 0);

	destroy_workqueue(ehci_wq);

	msm_hsic_config_gpios(mehci, 0);

	if (pdata && pdata->resume_gpio)
		gpio_free(pdata->resume_gpio);

	msm_hsic_init_vddcx(mehci, 0);
	msm_hsic_init_gdsc(mehci, 0);

	msm_hsic_init_clocks(mehci, 0);
	wake_lock_destroy(&mehci->wlock);
	iounmap(hcd->regs);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int msm_hsic_pm_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "ehci-msm-hsic PM suspend\n");

	dbg_log_event(NULL, "PM Suspend", 0);

	if (!atomic_read(&mehci->in_lpm)) {
		dev_info(dev, "abort suspend\n");
		dbg_log_event(NULL, "PM Suspend abort", 0);
		return -EBUSY;
	}

	if (device_may_wakeup(dev) && !mehci->async_irq)
		enable_irq_wake(hcd->irq);

	return 0;
}

static int msm_hsic_pm_suspend_noirq(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	if (atomic_read(&mehci->async_int)) {
		dev_dbg(dev, "suspend_noirq: Aborting due to pending interrupt\n");
		return -EBUSY;
	}

	return 0;
}

static int msm_hsic_pm_resume(struct device *dev)
{
	int ret;
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "ehci-msm-hsic PM resume\n");
	dbg_log_event(NULL, "PM Resume", 0);

	if (device_may_wakeup(dev) && !mehci->async_irq)
		disable_irq_wake(hcd->irq);

	/*
	 * Keep HSIC in Low Power Mode if system is resumed
	 * by any other wakeup source.  HSIC is resumed later
	 * when remote wakeup is received or interface driver
	 * start I/O.
	 */
	if (!atomic_read(&mehci->pm_usage_cnt) &&
			!atomic_read(&mehci->async_int) &&
			pm_runtime_suspended(dev))
		return 0;

	ret = msm_hsic_resume(mehci);
	if (ret)
		return ret;

	/* Bring the device to full powered state upon system resume */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int msm_hsic_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "EHCI runtime idle\n");
	return 0;
}

static int msm_hsic_runtime_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "EHCI runtime suspend\n");

	dbg_log_event(NULL, "Run Time PM Suspend", 0);

	return msm_hsic_suspend(mehci);
}

static int msm_hsic_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct msm_hsic_hcd *mehci = hcd_to_hsic(hcd);

	dev_dbg(dev, "EHCI runtime resume\n");

	dbg_log_event(NULL, "Run Time PM Resume", 0);

	return msm_hsic_resume(mehci);
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops msm_hsic_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_hsic_pm_suspend, msm_hsic_pm_resume)
	.suspend_noirq = msm_hsic_pm_suspend_noirq,
	SET_RUNTIME_PM_OPS(msm_hsic_runtime_suspend, msm_hsic_runtime_resume,
				msm_hsic_runtime_idle)
};
#endif
static const struct of_device_id hsic_host_dt_match[] = {
	{ .compatible = "qcom,hsic-host",
	},
	{}
};
static struct platform_driver ehci_msm_hsic_driver = {
	.probe	= ehci_hsic_msm_probe,
	.remove	= ehci_hsic_msm_remove,
	.driver = {
		.name = "msm_hsic_host",
#ifdef CONFIG_PM
		.pm = &msm_hsic_dev_pm_ops,
#endif
		.of_match_table = hsic_host_dt_match,
	},
};
