/* Goodix's GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M/GF3208
 * /GF5206/GF5216/GF5208
 * fingerprint sensor linux driver for TEE
 *
 * 2010 - 2015 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/fb.h>
//new added
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/pm_wakeup.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CLKMGR
#include "mach/mt_clkmgr.h"
#else
#include <linux/clk.h>
#endif

#include <net/sock.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

/* MTK header */
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif

#include "gf_spi_tee.h"
#include "gf_fw.h"

/**************************defination******************************/
#define GF_DEV_NAME "goodix_fp"
#define GF_DEV_MAJOR 0	/* assigned */

#define GF_CLASS_NAME "goodix_fp"
#define GF_INPUT_NAME "gf-keys"

#define GF_LINUX_VERSION "V1.01.04"

/* for GF test temporary, need defined in include/uapi/linux/netlink.h */
#define GF_NETLINK_ROUTE 30
#define MAX_NL_MSG_LEN 16

/*************************************************************/

/* debug log setting */
u8 g_debug_level = INFO_LOG;

/* align=2, 2 bytes align */
/* align=4, 4 bytes align */
/* align=8, 8 bytes align */
#define ROUND_UP(x, align)		((x+(align-1))&~(align-1))

u8	id_buf[11];

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
u32 rst_mt6306_support;
int rst_mt6306_gpio = -1;
#endif

/* for Upstream SPI ,just tell SPI about the clock */
#ifdef CONFIG_SPI_MT65XX
u32 gf_spi_speed = 1*1000000;
#endif

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct wakeup_source fp_wakeup_source;
static unsigned int bufsiz = (25 * 1024);
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "maximum data bytes for SPI message");

#ifdef CONFIG_OF
static const struct of_device_id gf_of_match[] = {
	{ .compatible = "mediatek,fingerprint", },
	{ .compatible = "mediatek,goodix-fp", },
	{ .compatible = "goodix,goodix-fp", },
	{},
};
MODULE_DEVICE_TABLE(of, gf_of_match);
#endif

/* for netlink use */
static int pid;

static u8 g_vendor_id;

static ssize_t gf_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t gf_debug_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);

static DEVICE_ATTR(debug, 0644, gf_debug_show, gf_debug_store);

static struct attribute *gf_debug_attrs[] = {
	&dev_attr_debug.attr,
	NULL
};

static const struct attribute_group gf_debug_attr_group = {
	.attrs = gf_debug_attrs,
	.name = "debug"
};
#ifndef CONFIG_SPI_MT65XX
const struct mt_chip_conf spi_ctrldata = {
	.setuptime = 10,
	.holdtime = 10,
	.high_time = 50, /* 1MHz */
	.low_time = 50,
	.cs_idletime = 10,
	.ulthgh_thrsh = 0,

	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,

	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,

	.tx_endian = SPI_LENDIAN,
	.rx_endian = SPI_LENDIAN,

	.com_mod = FIFO_TRANSFER,
	/* .com_mod = DMA_TRANSFER, */

	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif
/* -------------------------------------------------------------------- */
/* timer function							*/
/* -------------------------------------------------------------------- */
#define TIME_START	   0
#define TIME_STOP	   1

static long int prev_time, cur_time;

long int kernel_time(unsigned int step)
{
	cur_time = ktime_to_us(ktime_get());
	if (step == TIME_START) {
		prev_time = cur_time;
		return 0;
	} else if (step == TIME_STOP) {
		gf_debug(DEBUG_LOG, "%s, use: %ld us\n",
			__func__, (cur_time - prev_time));
		return cur_time - prev_time;
	}
	prev_time = cur_time;
	return -1;
}

/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration				*/
/* -------------------------------------------------------------------- */
static int gf_get_gpio_dts_info(struct gf_device *gf_dev)
{
#ifdef CONFIG_OF
	int ret;
	int virq;

	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	node = of_find_compatible_node(NULL, NULL, "goodix,goodix-fp");
	of_property_read_u32(node, "mt6306-rst-support", &rst_mt6306_support);
	gf_debug(INFO_LOG, "%s line:%d mt6306-fpRst-support:%d\n",
			__func__, __LINE__, rst_mt6306_support);

	if (rst_mt6306_support == 1) {
		of_property_read_u32(node, "mt6306-rst-gpionum",
					&rst_mt6306_gpio);
		gf_debug(INFO_LOG,
			"%s:%d mt6306-gpionum:%d fingerprint reset.\n",
			__func__, __LINE__, rst_mt6306_gpio);
	}
#endif

	gf_debug(DEBUG_LOG, "%s, from dts pinctrl\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,goodix-fp");
	if (node) {
		virq = irq_of_parse_and_map(node, 0);
#ifndef CONFIG_MTK_EIC
		irq_set_irq_wake(virq, 1);
#else
		enable_irq_wake(virq);
#endif
		pdev = of_find_device_by_node(node);
		if (pdev) {
			gf_dev->pinctrl_gpio = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(gf_dev->pinctrl_gpio)) {
				ret = PTR_ERR(gf_dev->pinctrl_gpio);
				gf_debug(ERR_LOG,
				"%s can't find fingerprint pctrl\n", __func__);
				return ret;
			}
		} else {
			gf_debug(ERR_LOG, "%s platform device is null\n",
				__func__);
		}
	} else {
		gf_debug(ERR_LOG, "%s device node is null\n", __func__);
	}

	/* it's normal that get "default" will failed */
	gf_dev->pins_default =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "default");
	if (IS_ERR(gf_dev->pins_default)) {
		ret = PTR_ERR(gf_dev->pins_default);
		gf_debug(ERR_LOG, "%s pctrl default get failed\n", __func__);
		/* return ret; */
	}

#ifdef SUPPORT_REE_OSWEGO
	gf_dev->pins_miso_spi =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "miso_spi");
	if (IS_ERR(gf_dev->pins_miso_spi)) {
		ret = PTR_ERR(gf_dev->pins_miso_spi);
		gf_debug(ERR_LOG, "%s pctrl miso_spi get failed\n", __func__);
		return ret;
	}
	gf_dev->miso_pullhigh =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "miso_pullhigh");
	if (IS_ERR(gf_dev->miso_pullhigh)) {
		ret = PTR_ERR(gf_dev->miso_pullhigh);
		gf_debug(ERR_LOG,
			"%s pinctrl miso_pullhigh get failed\n", __func__);
		return ret;
	}
	gf_dev->miso_pulllow =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "miso_pulllow");
	if (IS_ERR(gf_dev->miso_pulllow)) {
		ret = PTR_ERR(gf_dev->miso_pulllow);
		gf_debug(ERR_LOG,
			"%s pinctrl miso_pulllow get failed\n", __func__);
		return ret;
	}
#endif

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	if (rst_mt6306_support != 1) {
#endif
		gf_dev->rst_high =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "reset_high");

		if (IS_ERR(gf_dev->rst_high)) {
			ret = PTR_ERR(gf_dev->rst_high);
			gf_debug(ERR_LOG,
				"%s pinctrl reset_high get fail\n", __func__);
			return ret;
		}
		gf_dev->rst_low =
		pinctrl_lookup_state(gf_dev->pinctrl_gpio, "reset_low");
		if (IS_ERR(gf_dev->rst_low)) {
			ret = PTR_ERR(gf_dev->rst_low);
			gf_debug(ERR_LOG,
				"%s pinctrl reset_low get fail\n", __func__);
			return ret;
		}
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	}
#endif
	gf_dev->eint_init =
	pinctrl_lookup_state(gf_dev->pinctrl_gpio, "eint_init");
	if (IS_ERR(gf_dev->eint_init)) {
		ret = PTR_ERR(gf_dev->eint_init);
		gf_debug(ERR_LOG,
			"%s pinctrl eint_init get fail\n", __func__);
		return ret;
	}
	pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->eint_init);

	gf_debug(DEBUG_LOG, "%s, get pinctrl success!\n", __func__);

#endif
	return 0;
}

static int gf_get_sensor_dts_info(void)
{
	struct device_node *node = NULL;
	int value;

	node = of_find_compatible_node(NULL, NULL, "goodix,goodix-fp");
	if (node) {
		of_property_read_u32(node, "netlink-event", &value);
		gf_debug(DEBUG_LOG, "%s, get netlink event[%d] from dts\n",
				__func__, value);
	} else {
		gf_debug(ERR_LOG, "%s get device node failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void gf_hw_power_enable(struct gf_device *gf_dev, u8 onoff)
{
	/* TODO: LDO configure */
	static int enable = 1;

	if (onoff && enable) {
	/* TODO:  set power  according to actual situation  */
	/* hwPowerOn(MT6331_POWER_LDO_VIBR, VOL_2800, "fingerprint"); */
		enable = 0;
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
		gf_debug(INFO_LOG, "%s line:%d\n", __func__, __LINE__);
	if (rst_mt6306_support == 1) {
		mt6306_set_gpio_out(rst_mt6306_gpio, MT6306_GPIO_OUT_LOW);
		mdelay(15);
		mt6306_set_gpio_out(rst_mt6306_gpio, MT6306_GPIO_OUT_HIGH);
		return;
		}
#endif
		#ifdef CONFIG_OF
		pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->rst_low);
		mdelay(15);
		pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->rst_high);
		#endif
	} else if (!onoff && !enable) {
		/* hwPowerDown(MT6331_POWER_LDO_VIBR, "fingerprint"); */
		enable = 1;
	}
}

static void gf_spi_clk_enable(struct gf_device *gf_dev, u8 bonoff)
{
#ifdef CONFIG_MTK_CLKMGR
	if (bonoff)
		enable_clock(MT_CG_PERI_SPI0, "spi");
	else
		disable_clock(MT_CG_PERI_SPI0, "spi");

#else
	static int count;

	if (bonoff && (count == 0)) {
		mt_spi_enable_master_clk(gf_dev->spi);
		count = 1;
	} else if ((count > 0) && (bonoff == 0)) {
		mt_spi_disable_master_clk(gf_dev->spi);
		count = 0;
	}
#endif
}

static void gf_bypass_flash_gpio_cfg(void)
{
	/* TODO: by pass flash IO config, default connect to GND */
}

static void gf_irq_gpio_cfg(struct gf_device *gf_dev)
{
#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,goodix-fp");
	if (node) {
		gf_dev->irq_num = irq_of_parse_and_map(node, 0);
		gf_debug(INFO_LOG, "%s, gf_irq = %d\n",
			__func__, gf_dev->irq_num);
		gf_dev->irq = gf_dev->irq_num;
	} else
		gf_debug(ERR_LOG, "%s get compatible node fail\n", __func__);

#endif
}

static void gf_reset_gpio_cfg(struct gf_device *gf_dev)
{
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	if (rst_mt6306_support == 1) {
		mt6306_set_gpio_out(rst_mt6306_gpio, MT6306_GPIO_OUT_HIGH);
		return;
	}
#endif

#ifdef CONFIG_OF
	pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->rst_high);
#endif

}

/* delay ms after reset */
static void gf_hw_reset(struct gf_device *gf_dev, u8 delay)
{
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	if (rst_mt6306_support == 1) {
		mt6306_set_gpio_out(rst_mt6306_gpio, MT6306_GPIO_OUT_LOW);
		mdelay(5);
		mt6306_set_gpio_out(rst_mt6306_gpio, MT6306_GPIO_OUT_HIGH);

		if (delay) {
			/* delay is configurable */
			mdelay(delay);
		}
		return;
	}
#endif

#ifdef CONFIG_OF
	pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->rst_low);
	mdelay(5);
	pinctrl_select_state(gf_dev->pinctrl_gpio, gf_dev->rst_high);
#endif

	if (delay) {
		/* delay is configurable */
		mdelay(delay);
	}
}

static void gf_enable_irq(struct gf_device *gf_dev)
{
	if (gf_dev->irq_count == 1) {
		gf_debug(ERR_LOG, "%s, irq already enabled\n", __func__);
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_count = 1;
		gf_debug(DEBUG_LOG, "%s enable interrupt!\n", __func__);
	}
}

static void gf_disable_irq(struct gf_device *gf_dev)
{
	if (gf_dev->irq_count == 0) {
		gf_debug(ERR_LOG, "%s, irq already disabled\n", __func__);
	} else {
		disable_irq(gf_dev->irq);
		gf_dev->irq_count = 0;
		gf_debug(DEBUG_LOG, "%s disable interrupt!\n", __func__);
	}
}


/* -------------------------------------------------------------------- */
/* netlink functions                 */
/* -------------------------------------------------------------------- */
void gf_netlink_send(struct gf_device *gf_dev, const int command)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int ret;

	gf_debug(INFO_LOG, "[%s]:enter,send command %d\n", __func__, command);
	if (gf_dev->nl_sk == NULL) {
		gf_debug(ERR_LOG, "[%s]:invalid socket\n", __func__);
		return;
	}

	if (pid == 0) {
		gf_debug(ERR_LOG, "[%s]invalid native process pid\n", __func__);
		return;
	}

	/*alloc data buffer for sending to native*/
	/*malloc data space at least 1500 bytes,which is ethernet data length*/
	skb = alloc_skb(MAX_NL_MSG_LEN, GFP_ATOMIC);
	if (skb == NULL)
		return;

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_NL_MSG_LEN, 0);
	if (!nlh) {
		gf_debug(ERR_LOG, "[%s]nlmsg_put failed\n", __func__);
		kfree_skb(skb);
		return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	*(char *)NLMSG_DATA(nlh) = command;
	ret = netlink_unicast(gf_dev->nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret == 0) {
		gf_debug(ERR_LOG, "[%s] : send failed\n", __func__);
		return;
	}

	gf_debug(INFO_LOG, "[%s]send done,data length: %d\n", __func__, ret);
}

static void gf_netlink_recv(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	char str[128];

	gf_debug(INFO_LOG, "[%s] : enter\n", __func__);

	skb = skb_get(__skb);
	if (skb == NULL) {
		gf_debug(ERR_LOG, "[%s] : skb_get return NULL\n", __func__);
		return;
	}

	/* presume there is 5byte payload at leaset */
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);
		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;
		gf_debug(INFO_LOG, "[%s]pid:%d,msg:%s\n", __func__, pid, str);

	} else {
		gf_debug(ERR_LOG, "[%s]:not enough data length\n", __func__);
	}

	kfree_skb(skb);
}

static int gf_netlink_init(struct gf_device *gf_dev)
{
	struct netlink_kernel_cfg cfg;

	memset(&cfg, 0, sizeof(struct netlink_kernel_cfg));
	cfg.input = gf_netlink_recv;

	gf_dev->nl_sk =
		netlink_kernel_create(&init_net, GF_NETLINK_ROUTE, &cfg);
	if (gf_dev->nl_sk == NULL) {
		gf_debug(ERR_LOG, "[%s]: netlink create failed\n", __func__);
		return -1;
	}

	gf_debug(INFO_LOG, "[%s]: netlink create success\n", __func__);
	return 0;
}

static int gf_netlink_destroy(struct gf_device *gf_dev)
{
	if (gf_dev->nl_sk != NULL) {
		netlink_kernel_release(gf_dev->nl_sk);
		gf_dev->nl_sk = NULL;
		return 0;
	}

	gf_debug(ERR_LOG, "[%s] : no netlink socket yet\n", __func__);
	return -1;
}

/* -------------------------------------------------------------------- */
/* early suspend callback and suspend/resume functions          */
/* -------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void gf_early_suspend(struct early_suspend *handler)
{
	struct gf_device *gf_dev = NULL;

	gf_dev = container_of(handler, struct gf_device, early_suspend);
	gf_debug(INFO_LOG, "[%s] enter\n", __func__);

	gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_OFF);
}

static void gf_late_resume(struct early_suspend *handler)
{
	struct gf_device *gf_dev = NULL;

	gf_dev = container_of(handler, struct gf_device, early_suspend);
	gf_debug(INFO_LOG, "[%s] enter\n", __func__);

	gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_ON);
}
#else

static int gf_fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct gf_device *gf_dev = NULL;
	struct fb_event *evdata = data;
	unsigned int blank;
	int retval = 0;

	FUNC_ENTRY();

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */)
		return 0;

	gf_dev = container_of(self, struct gf_device, notifier);
	blank = *(int *)evdata->data;

	gf_debug(INFO_LOG, "[%s] : enter, blank=0x%x\n", __func__, blank);

	switch (blank) {
	case FB_BLANK_UNBLANK:
		gf_debug(INFO_LOG, "[%s] : lcd on notify\n", __func__);
		gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_ON);
		break;

	case FB_BLANK_POWERDOWN:
		gf_debug(INFO_LOG, "[%s] : lcd off notify\n", __func__);
		gf_netlink_send(gf_dev, GF_NETLINK_SCREEN_OFF);
		break;

	default:
		gf_debug(INFO_LOG, "[%s]:other notifier,ignore\n", __func__);
		break;
	}
	FUNC_EXIT();
	return retval;
}

#endif /* CONFIG_HAS_EARLYSUSPEND */

/* -------------------------------------------------------------------- */
/* file operation function                                              */
/* -------------------------------------------------------------------- */
static ssize_t gf_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	int retval = 0;

#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_OSWEGO
	struct gf_device *gf_dev = NULL;
	u8 status;
	u8 *transfer_buf = NULL;
	u16 checksum = 0;
	u16 tmpcheck = 0;
	int i = 0;

	FUNC_ENTRY();
	gf_dev = (struct gf_device *)filp->private_data;

	gf_spi_read_byte_ree(gf_dev, 0x8140, &status);
	if ((status & 0xF0) != 0xC0) {
		gf_debug(ERR_LOG, "%s: no image data available\n", __func__);
		return 0;
	}
	if ((count > bufsiz) || (count == 0)) {
		gf_debug(ERR_LOG,
		"%s: request transfer length larger than maximum buffer\n",
		__func__);
		return -EINVAL;
	}

	transfer_buf = kzalloc((count + 10), GFP_KERNEL);
	if (transfer_buf == NULL)
		return -EMSGSIZE;

	/* set spi to high speed */
#ifndef CONFIG_SPI_MT65XX
	gf_spi_setup_conf_ree(gf_dev, HIGH_SPEED, DMA_TRANSFER);
#else
	gf_spi_speed = 6*1000000;
#endif

	gf_spi_read_bytes_ree(gf_dev, 0x8140, count + 10, transfer_buf);

	/* check checksum */
	checksum = 0;
	for (i = 0; i < (count + 6); i++)
		checksum += *(transfer_buf + 2 + i);

	tmpcheck = (*(transfer_buf + count + 8) << 8);
	tmpcheck |= (*(transfer_buf + count + 9));
	if (checksum != tmpcheck) {
		gf_debug(ERR_LOG,
		"%s:raw data checksum failed,cal[0x%x],recevied[0x%x]\n",
		__func__, checksum, tmpcheck);
		retval = 0;
	} else {
		gf_debug(INFO_LOG,
			"%s:checksum passed[0x%x]\n",
			__func__, checksum);
		if (copy_to_user(buf, transfer_buf + 8, count)) {
			gf_debug(ERR_LOG, "%s:copy_to_user fail\n", __func__);
			retval = -EFAULT;
		} else {
			retval = count;
		}
	}

	/* restore to low speed */
#ifndef CONFIG_SPI_MT65XX
	gf_spi_setup_conf_ree(gf_dev, LOW_SPEED, FIFO_TRANSFER);
#else
	gf_spi_speed = 1*1000000;
#endif

	kfree(transfer_buf);
#endif
#endif /* SUPPORT_REE_SPI */

	FUNC_EXIT();
	return retval;
}

static ssize_t gf_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos)
{
	gf_debug(ERR_LOG, "%s:Not support write opertion in TEE mode\n",
			__func__);
	return -EFAULT;
}

static irqreturn_t gf_irq(int irq, void *handle)
{
	struct gf_device *gf_dev = (struct gf_device *)handle;

	FUNC_ENTRY();

	__pm_wakeup_event(&fp_wakeup_source, 500);

	gf_netlink_send(gf_dev, GF_NETLINK_IRQ);
	gf_dev->sig_count++;

	FUNC_EXIT();
	return IRQ_HANDLED;
}


static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_device *gf_dev = NULL;
	struct gf_key gf_key;
	enum gf_nav_event_t nav_event = GF_NAV_NONE;
	uint32_t nav_input = 0;
	uint32_t key_input = 0;
#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_OSWEGO
	struct gf_ioc_transfer ioc;
	u8 *transfer_buf = NULL;
#endif
#endif
	int retval = 0;
	int ret = 0;
	u8  buf    = 0;
	u8 netlink_route = GF_NETLINK_ROUTE;
	struct gf_ioc_chip_info info;
	void __user *data;

	FUNC_ENTRY();
	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -EINVAL;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));

	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		retval = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));

	if (retval)
		return -EINVAL;

	gf_dev = (struct gf_device *)filp->private_data;
	if (!gf_dev) {
		gf_debug(ERR_LOG, "%s: gf_dev IS NULL ===\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case GF_IOC_INIT:
		gf_debug(INFO_LOG, "%s: GF_IOC_INIT gf init===\n", __func__);
		gf_debug(INFO_LOG, "%s: Linux Version %s\n",
				__func__, GF_LINUX_VERSION);
		ret = copy_to_user((void __user *)arg,
					(void *)&netlink_route, sizeof(u8));
		if (ret) {
			retval = -EFAULT;
			break;
		}

		if (gf_dev->system_status) {
			gf_debug(INFO_LOG, "%s:system re-start\n", __func__);
			break;
		}
		gf_irq_gpio_cfg(gf_dev);
		retval = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"goodix_fp_irq", gf_dev);
		if (!retval)
			gf_debug(INFO_LOG, "%s irq thread request success\n",
					__func__);
		else
			gf_debug(ERR_LOG,
				"%s irq thread request failed, retval=%d\n",
				__func__, retval);

		gf_dev->irq_count = 1;
		gf_disable_irq(gf_dev);

#if defined(CONFIG_HAS_EARLYSUSPEND)
		gf_debug(INFO_LOG, "[%s]:register_early_suspend\n", __func__);
		ret = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
		gf_dev->early_suspend.level = ret;
		gf_dev->early_suspend.suspend = gf_early_suspend,
		gf_dev->early_suspend.resume = gf_late_resume,
		register_early_suspend(&gf_dev->early_suspend);
#else
		/* register screen on/off callback */
		gf_dev->notifier.notifier_call = gf_fb_notifier_callback;
		retval = fb_register_client(&gf_dev->notifier);
		if (retval)
			gf_debug(ERR_LOG, "%s register fb failed,retval=%d\n",
				__func__, retval);
#endif

		gf_dev->sig_count = 0;
		gf_dev->system_status = 1;

		gf_debug(INFO_LOG, "%s: gf init finished===\n", __func__);
		break;

	case GF_IOC_CHIP_INFO:
		ret = copy_from_user(&info, (struct gf_ioc_chip_info *)arg,
					sizeof(struct gf_ioc_chip_info));
		if (ret) {
			retval = -EFAULT;
			break;
		}
		g_vendor_id = info.vendor_id;

		gf_debug(INFO_LOG, "%s: vendor_id 0x%x\n",
				__func__, g_vendor_id);
		gf_debug(INFO_LOG, "%s: mode 0x%x\n", __func__, info.mode);
		gf_debug(INFO_LOG, "%s: operation 0x%x\n",
				__func__, info.operation);
		break;

	case GF_IOC_EXIT:
		gf_debug(INFO_LOG, "%s: GF_IOC_EXIT ===\n", __func__);
		gf_disable_irq(gf_dev);
		if (gf_dev->irq) {
			free_irq(gf_dev->irq, gf_dev);
			gf_dev->irq_count = 0;
			gf_dev->irq = 0;
		}

#ifdef CONFIG_HAS_EARLYSUSPEND
		if (gf_dev->early_suspend.suspend)
			unregister_early_suspend(&gf_dev->early_suspend);
#else
		fb_unregister_client(&gf_dev->notifier);
#endif

		gf_dev->system_status = 0;
		gf_debug(INFO_LOG, "%s: gf exit finished ===\n", __func__);
		break;

	case GF_IOC_RESET:
		gf_debug(INFO_LOG, "%s: chip reset command\n", __func__);
		gf_hw_reset(gf_dev, 60);
		break;

	case GF_IOC_ENABLE_IRQ:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_IRQ ===\n", __func__);
		gf_enable_irq(gf_dev);
		break;

	case GF_IOC_DISABLE_IRQ:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_IRQ ===\n", __func__);
		gf_disable_irq(gf_dev);
		break;

	case GF_IOC_ENABLE_SPI_CLK:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_SPI_CLK ==\n", __func__);
		gf_spi_clk_enable(gf_dev, 1);
		break;

	case GF_IOC_DISABLE_SPI_CLK:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_SPI_CLK =\n", __func__);
		gf_spi_clk_enable(gf_dev, 0);
		break;

	case GF_IOC_ENABLE_POWER:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENABLE_POWER ==\n", __func__);
		gf_hw_power_enable(gf_dev, 1);
		break;

	case GF_IOC_DISABLE_POWER:
		gf_debug(INFO_LOG, "%s: GF_IOC_DISABLE_POWER =\n", __func__);
		gf_hw_power_enable(gf_dev, 0);
		break;

	case GF_IOC_INPUT_KEY_EVENT:
		ret = copy_from_user(&gf_key, (struct gf_key *)arg,
					sizeof(struct gf_key));
		if (ret) {
			gf_debug(ERR_LOG,
			"Failed to copy key event from user to kernel\n");
			retval = -EFAULT;
			break;
		}

		if (gf_key.key == GF_KEY_HOME) {
			key_input = GF_KEY_INPUT_HOME;
		} else if (gf_key.key == GF_KEY_POWER) {
			key_input = GF_KEY_INPUT_POWER;
		} else if (gf_key.key == GF_KEY_CAMERA) {
			key_input = GF_KEY_INPUT_CAMERA;
		} else {
			/* add special key define */
			key_input = gf_key.key;
		}
		gf_debug(INFO_LOG, "%s:received event[%d],key=%d,value=%d\n",
				__func__, key_input, gf_key.key, gf_key.value);

		if ((gf_key.key == GF_KEY_POWER || gf_key.key == GF_KEY_CAMERA)
			&& (gf_key.value == 1)) {
			input_report_key(gf_dev->input, key_input, 1);
			input_sync(gf_dev->input);
			input_report_key(gf_dev->input, key_input, 0);
			input_sync(gf_dev->input);
		}

		if (gf_key.key == GF_KEY_HOME) {
			input_report_key(gf_dev->input,
				key_input, gf_key.value);
			input_sync(gf_dev->input);
		}

		break;

	case GF_IOC_NAV_EVENT:
	    gf_debug(ERR_LOG, "nav event");
		if (copy_from_user(&nav_event, (enum gf_nav_event_t *)arg,
					sizeof(enum gf_nav_event_t))) {
			gf_debug(ERR_LOG,
				"nav event copy_from_user failed\n");
			retval = -EFAULT;
			break;
		}

		switch (nav_event) {
		case GF_NAV_FINGER_DOWN:
			gf_debug(ERR_LOG, "nav finger down");
			break;

		case GF_NAV_FINGER_UP:
			gf_debug(ERR_LOG, "nav finger up");
			break;

		case GF_NAV_DOWN:
			nav_input = GF_NAV_INPUT_DOWN;
			gf_debug(ERR_LOG, "nav down");
			break;

		case GF_NAV_UP:
			nav_input = GF_NAV_INPUT_UP;
			gf_debug(ERR_LOG, "nav up");
			break;

		case GF_NAV_LEFT:
			nav_input = GF_NAV_INPUT_LEFT;
			gf_debug(ERR_LOG, "nav left");
			break;

		case GF_NAV_RIGHT:
			nav_input = GF_NAV_INPUT_RIGHT;
			gf_debug(ERR_LOG, "nav right");
			break;

		case GF_NAV_CLICK:
			nav_input = GF_NAV_INPUT_CLICK;
			gf_debug(ERR_LOG, "nav click");
			break;

		case GF_NAV_HEAVY:
			nav_input = GF_NAV_INPUT_HEAVY;
			break;

		case GF_NAV_LONG_PRESS:
			nav_input = GF_NAV_INPUT_LONG_PRESS;
			break;

		case GF_NAV_DOUBLE_CLICK:
			nav_input = GF_NAV_INPUT_DOUBLE_CLICK;
			break;

		default:
			gf_debug(INFO_LOG,
				"%s: not support nav event nav_event: %d\n",
				__func__, nav_event);
			break;
		}

		if ((nav_event != GF_NAV_FINGER_DOWN)
			&& (nav_event != GF_NAV_FINGER_UP)) {
			input_report_key(gf_dev->input, nav_input, 1);
			input_sync(gf_dev->input);
			input_report_key(gf_dev->input, nav_input, 0);
			input_sync(gf_dev->input);
		}
		break;

	case GF_IOC_ENTER_SLEEP_MODE:
		gf_debug(INFO_LOG, "%s: GF_IOC_ENTER_SLEEP_MODE\n", __func__);
		break;

	case GF_IOC_GET_FW_INFO:
		gf_debug(INFO_LOG, "%s: GF_IOC_GET_FW_INFO ==\n", __func__);
		buf = gf_dev->need_update;

		gf_debug(DEBUG_LOG, "%s: firmware info 0x%x\n", __func__, buf);
		if (copy_to_user((void __user *)arg,
			(void *)&buf, sizeof(u8))) {
			gf_debug(ERR_LOG, "Failed to copy data to user\n");
			retval = -EFAULT;
		}

		break;
	case GF_IOC_REMOVE:
#if 0
		gf_debug(INFO_LOG, "%s: GF_IOC_REMOVE ===\n", __func__);

		gf_netlink_destroy(gf_dev);

		mutex_lock(&gf_dev->release_lock);
		if (gf_dev->input == NULL) {
			mutex_unlock(&gf_dev->release_lock);
			break;
		}
		input_unregister_device(gf_dev->input);
		gf_dev->input = NULL;
		mutex_unlock(&gf_dev->release_lock);

		cdev_del(&gf_dev->cdev);
		sysfs_remove_group(&gf_dev->spi->dev.kobj,
			&gf_debug_attr_group);
		device_destroy(gf_dev->class, gf_dev->devno);
		list_del(&gf_dev->device_entry);
		unregister_chrdev_region(gf_dev->devno, 1);
		class_destroy(gf_dev->class);
		gf_hw_power_enable(gf_dev, 0);
		gf_spi_clk_enable(gf_dev, 0);

		mutex_lock(&gf_dev->release_lock);
		if (gf_dev->spi_buffer != NULL) {
			kfree(gf_dev->spi_buffer);
			gf_dev->spi_buffer = NULL;
		}
		mutex_unlock(&gf_dev->release_lock);

		spi_set_drvdata(gf_dev->spi, NULL);
		gf_dev->spi = NULL;
		mutex_destroy(&gf_dev->buf_lock);
		mutex_destroy(&gf_dev->release_lock);
#endif
		break;
	case GF_IOC_FTM:
		data = (void __user *) arg;
		if (copy_to_user(data, id_buf, 7)) {
			retval = -EFAULT;
			break;
		}
		gf_debug(INFO_LOG, "%s: GF_IOC_FTM ===\n", __func__);
		break;

#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_OSWEGO
	case GF_IOC_TRANSFER_CMD:
		if (copy_from_user(&ioc, (struct gf_ioc_transfer *)arg,
			sizeof(struct gf_ioc_transfer))) {
			gf_debug(ERR_LOG,
				"%s:Failed copy gf_ioc_transfer to kernel\n",
				__func__);
			retval = -EFAULT;
			break;
		}

		if ((ioc.len > bufsiz) || (ioc.len == 0)) {
			gf_debug(ERR_LOG,
				"%s: transfer len larger than max buffer\n",
				__func__);
			retval = -EINVAL;
			break;
		}
		transfer_buf = kzalloc(ioc.len, GFP_KERNEL);
		if (transfer_buf == NULL) {
			retval = -EMSGSIZE;
			break;
		}

		mutex_lock(&gf_dev->buf_lock);
		if (ioc.cmd) {
			/* spi write operation */
			gf_debug(DEBUG_LOG, "%s:write data 0x%x,len = 0x%x\n",
				__func__, ioc.addr, ioc.len);
			if (copy_from_user(transfer_buf, ioc.buf, ioc.len)) {
				gf_debug(ERR_LOG,
					"ioc_transfer copy_from_user fail\n");
				retval = -EFAULT;
			} else {
				gf_spi_write_bytes_ree(gf_dev,
					ioc.addr, ioc.len, transfer_buf);
			}
		} else {
			/* spi read operation */
			gf_debug(DEBUG_LOG, "%s:data addr 0x%x,len=0x%x\n",
				__func__, ioc.addr, ioc.len);
			gf_spi_read_bytes_ree(gf_dev,
				ioc.addr, ioc.len, transfer_buf);
			if (copy_to_user(ioc.buf, transfer_buf, ioc.len)) {
				gf_debug(ERR_LOG,
					"ioc.buf copy_to_user failed\n");
				retval = -EFAULT;
			}
		}
		kfree(transfer_buf);
		mutex_unlock(&gf_dev->buf_lock);
		break;
#endif

	case GF_IOC_TRANSFER_RAW_CMD:
		retval = gf_ioctl_transfer_raw_cmd(gf_dev, arg, bufsiz);
		break;

	case GF_IOC_SPI_INIT_CFG_CMD:
#ifndef CONFIG_SPI_MT65XX
		retval = gf_ioctl_spi_init_cfg_cmd(&gf_dev->spi_mcc, arg);
#endif
		break;

#endif /* SUPPORT_REE_SPI */
	default:
		gf_debug(ERR_LOG, "gf don't support the command(%x)\n", cmd);
		break;
	}

	FUNC_EXIT();
	return retval;
}

#ifdef CONFIG_COMPAT
static long gf_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	FUNC_ENTRY();

	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);

	FUNC_EXIT();
	return retval;
}
#endif

static unsigned int gf_poll(struct file *filp, struct poll_table_struct *wait)
{
	gf_debug(ERR_LOG, "Not support poll opertion in TEE version\n");
	return -EFAULT;
}


/* -------------------------------------------------------------------- */
/* devfs                                                              */
/* -------------------------------------------------------------------- */
static ssize_t gf_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	gf_debug(INFO_LOG, "%s: Show debug_level = 0x%x\n",
		__func__, g_debug_level);
	return sprintf(buf, "vendor id 0x%x\n", g_vendor_id);
}

static ssize_t gf_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct gf_device *gf_dev =  dev_get_drvdata(dev);
	int retval = 0;
#ifdef SUPPORT_REE_OSWEGO
	u8 flag = 0;
#endif

#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_MILAN_A
	u8 id_tmp[2] = {0};
#ifndef CONFIG_TRUSTONIC_TEE_SUPPORT
	u16 chip_id;
#endif
#endif
#endif

	if (!strncmp(buf, "-8", 2)) {
		gf_debug(INFO_LOG, "%s: para:-8,en spi clk test\n", __func__);
		mt_spi_enable_master_clk(gf_dev->spi);

	} else if (!strncmp(buf, "-9", 2)) {
		gf_debug(INFO_LOG, "%s: para:-9,en spi clk test\n", __func__);
		mt_spi_disable_master_clk(gf_dev->spi);

	} else if (!strncmp(buf, "-10", 3)) {
		gf_debug(INFO_LOG, "%s: para:-10, gf init start\n", __func__);

		gf_irq_gpio_cfg(gf_dev);
		retval = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				dev_name(&(gf_dev->spi->dev)), gf_dev);
		if (!retval)
			gf_debug(INFO_LOG, "%s irq thread request success!\n",
					__func__);
		else
			gf_debug(ERR_LOG, "%s irq thread failed, retval=%d\n",
					__func__, retval);

		gf_dev->irq_count = 1;
		gf_disable_irq(gf_dev);

#if defined(CONFIG_HAS_EARLYSUSPEND)
		gf_debug(INFO_LOG, "[%s]:register_early_suspend\n", __func__);
		gf_dev->early_suspend.level =
			(EARLY_SUSPEND_LEVEL_DISABLE_FB - 1);
		gf_dev->early_suspend.suspend = gf_early_suspend,
		gf_dev->early_suspend.resume = gf_late_resume,
		register_early_suspend(&gf_dev->early_suspend);
#else
		/* register screen on/off callback */
		gf_dev->notifier.notifier_call = gf_fb_notifier_callback;
		retval = fb_register_client(&gf_dev->notifier);
		if (retval)
			gf_debug(ERR_LOG, "%s register fb failed,retval=%d\n",
				__func__, retval);
#endif

		gf_dev->sig_count = 0;

		gf_debug(INFO_LOG, "%s: gf init finished ===\n", __func__);

	} else if (!strncmp(buf, "-11", 3)) {
		gf_debug(INFO_LOG, "%s: para is -11, enable irq\n", __func__);
		gf_enable_irq(gf_dev);

	} else if (!strncmp(buf, "-12", 3)) {
		gf_debug(INFO_LOG, "%s: para is -12, GPIO test\n", __func__);
		gf_reset_gpio_cfg(gf_dev);

#ifdef CONFIG_OF
#ifdef SUPPORT_REE_OSWEGO
		if (flag == 0) {
			pinctrl_select_state(gf_dev->pinctrl_gpio,
				gf_dev->miso_pulllow);
			gf_debug(INFO_LOG, "%s:miso PIN set low\n", __func__);
			flag = 1;
		} else {
			pinctrl_select_state(gf_dev->pinctrl_gpio,
				gf_dev->miso_pullhigh);
			gf_debug(INFO_LOG, "%s:miso PIN set high\n", __func__);
			flag = 0;
		}
#endif
#endif

	} else if (!strncmp(buf, "-13", 3)) {
		gf_debug(INFO_LOG, "%s: para: -13, Vendor ID test --> 0x%x\n",
			__func__, g_vendor_id);
	} else if (!strncmp(buf, "-15", 3)) {
#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_MILAN_A
		gf_spi_read_bytes_ree(gf_dev, 0x0142, 2, id_tmp);
		gf_debug(INFO_LOG, "%s line:%d ChipID:0x%x  0x%x\n",
			__func__, __LINE__, id_tmp[0], id_tmp[1]);

		/* make fingerprint to lower power mode for nonTEE project */
#ifndef CONFIG_TRUSTONIC_TEE_SUPPORT
		gf_spi_read_bytes_ree_new(gf_dev, 0x0142, 2, id_tmp);
		chip_id = (u16)id_tmp[0];
		chip_id += ((u16)id_tmp[1]) << 8;
		gf_debug(INFO_LOG, "[%s]id[0]=0x%x,id[1]=0x%x,chip:0x%x\n",
			__func__, id_tmp[0], id_tmp[1], chip_id);

		if (0x12A4 == chip_id || 0x12A1 == chip_id) {
			gf_debug(INFO_LOG,
				"[%s]%d, no TEE support,make the dev sleep\n",
				__func__, __LINE__);
			gf_milan_a_series_init_process(gf_dev);
		}
#endif
#endif
#endif
	} else {
		gf_debug(ERR_LOG, "%s: wrong parameter!==\n", __func__);
	}

	return count;
}

/* -------------------------------------------------------------------- */
/* device function						  */
/* -------------------------------------------------------------------- */
static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;
	int status = -ENXIO;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devno == inode->i_rdev) {
			gf_debug(INFO_LOG, "%s, Found\n", __func__);
			status = 0;
			break;
		}
	}
	mutex_unlock(&device_list_lock);

	if (status == 0) {
		filp->private_data = gf_dev;
		nonseekable_open(inode, filp);
		gf_debug(INFO_LOG, "%s, Success to open device. irq = %d\n",
			__func__, gf_dev->irq);
	} else {
		gf_debug(ERR_LOG, "%s, No device for minor %d\n",
			__func__, iminor(inode));
	}
	FUNC_EXIT();
	return status;
}

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_device *gf_dev = NULL;
	int    status = 0;

	FUNC_ENTRY();
	gf_dev = filp->private_data;
	if (gf_dev->irq)
		gf_disable_irq(gf_dev);
	gf_dev->need_update = 0;
	FUNC_EXIT();
	return status;
}

#ifdef SUPPORT_REE_SPI
int gf_spi_read_bytes_ree(struct gf_device *gf_dev,
				u16 addr, u32 data_len, u8 *rx_buf)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	u8 *tmp_buf = NULL;
	u32 package, reminder, retry;

	package = (data_len + 2) / 1024;
	reminder = (data_len + 2) % 1024;

	if ((package > 0) && (reminder != 0)) {
		xfer = kzalloc(sizeof(*xfer) * 4, GFP_KERNEL);
		retry = 1;
	} else {
		xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
		retry = 0;
	}
	if (xfer == NULL) {
		gf_debug(ERR_LOG, "%s,no memory for SPI transfer\n", __func__);
		return -ENOMEM;
	}

	tmp_buf = gf_dev->spi_buffer;

	/* switch to DMA mode if transfer length larger than 32 bytes */

#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 1) > 32) {
		gf_dev->spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif
	spi_message_init(&msg);
	*tmp_buf = 0xF0;
	*(tmp_buf + 1) = (u8)((addr >> 8) & 0xFF);
	*(tmp_buf + 2) = (u8)(addr & 0xFF);
	xfer[0].tx_buf = tmp_buf;
	xfer[0].len = 3;
#ifdef CONFIG_SPI_MT65XX
	xfer[0].speed_hz = gf_spi_speed;
	gf_debug(INFO_LOG, "%s %d, now spi-clock:%d\n",
			__func__, __LINE__, xfer[0].speed_hz);
#endif
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(gf_dev->spi, &msg);

	spi_message_init(&msg);
	/* memset((tmp_buf + 4), 0x00, data_len + 1); */
	/* 4 bytes align */
	*(tmp_buf + 4) = 0xF1;
	xfer[1].tx_buf = tmp_buf + 4;
	xfer[1].rx_buf = tmp_buf + 4;

	if (retry)
		xfer[1].len = package * 1024;
	else
		xfer[1].len = data_len + 1;
#ifdef CONFIG_SPI_MT65XX
	xfer[1].speed_hz = gf_spi_speed;
#endif
	xfer[1].delay_usecs = 5;
	spi_message_add_tail(&xfer[1], &msg);
	spi_sync(gf_dev->spi, &msg);

	/* copy received data */
	if (retry)
		memcpy(rx_buf, (tmp_buf + 5), (package * 1024 - 1));
	else
		memcpy(rx_buf, (tmp_buf + 5), data_len);

	/* send reminder SPI data */
	if (retry) {
		addr = addr + package * 1024 - 2;
		spi_message_init(&msg);

		*tmp_buf = 0xF0;
		*(tmp_buf + 1) = (u8)((addr >> 8) & 0xFF);
		*(tmp_buf + 2) = (u8)(addr & 0xFF);
		xfer[2].tx_buf = tmp_buf;
		xfer[2].len = 3;
#ifdef CONFIG_SPI_MT65XX
		xfer[2].speed_hz = gf_spi_speed;
#endif
		xfer[2].delay_usecs = 5;
		spi_message_add_tail(&xfer[2], &msg);
		spi_sync(gf_dev->spi, &msg);

		spi_message_init(&msg);
		*(tmp_buf + 4) = 0xF1;
		xfer[3].tx_buf = tmp_buf + 4;
		xfer[3].rx_buf = tmp_buf + 4;
		xfer[3].len = reminder + 1;
#ifdef CONFIG_SPI_MT65XX
		xfer[3].speed_hz = gf_spi_speed;
#endif
		xfer[3].delay_usecs = 5;
		spi_message_add_tail(&xfer[3], &msg);
		spi_sync(gf_dev->spi, &msg);

		memcpy((rx_buf + package * 1024 - 1),
				(tmp_buf + 6), (reminder - 1));
	}

	/* restore to FIFO mode if has used DMA */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 1) > 32) {
		gf_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}

#ifdef SUPPORT_REE_OSWEGO
static const char * const oswego_m_sensor_type[] = {
	"GF316M",
	"GF318M",
	"GF3118M",
	"GF518M",
	"GF5118M",
	"GF516M",
	"GF816M"
};

/* pull high miso, or change to SPI mode */
static void gf_miso_gpio_cfg(struct gf_device *gf_dev, u8 pullhigh)
{
#ifdef CONFIG_OF
	if (pullhigh)
		pinctrl_select_state(gf_dev->pinctrl_gpio,
					gf_dev->miso_pullhigh);
	else
		pinctrl_select_state(gf_dev->pinctrl_gpio,
					gf_dev->pins_miso_spi);

#endif
}

/* -------------------------------------------------------------------- */
/* normal world SPI read/write function                 */
/* -------------------------------------------------------------------- */

/* gf_spi_setup_conf_ree, configure spi speed and transfer mode in REE mode
 *
 * speed: 1, 4, 6, 8 unit:MHz
 * mode: DMA mode or FIFO mode
 */
 #ifndef CONFIG_SPI_MT65XX
void gf_spi_setup_conf_ree(struct gf_device *gf_dev, u32 speed,
				enum spi_transfer_mode mode)
{
	struct mt_chip_conf *mcc = &gf_dev->spi_mcc;

	switch (speed) {
	case 1:
		/* set to 1MHz clock */
		mcc->high_time = 50;
		mcc->low_time = 50;
		break;
	case 4:
		/* set to 4MHz clock */
		mcc->high_time = 15;
		mcc->low_time = 15;
		break;
	case 6:
		/* set to 6MHz clock */
		mcc->high_time = 10;
		mcc->low_time = 10;
		break;
	case 8:
		/* set to 8MHz clock */
		mcc->high_time = 8;
		mcc->low_time = 8;
		break;
	default:
		/* default set to 1MHz clock */
		mcc->high_time = 50;
		mcc->low_time = 50;
	}

	if ((mode == DMA_TRANSFER) || (mode == FIFO_TRANSFER)) {
		mcc->com_mod = mode;
	} else {
		/* default set to FIFO mode */
		mcc->com_mod = FIFO_TRANSFER;
	}

	if (spi_setup(gf_dev->spi))
		gf_debug(ERR_LOG, "%s,failed to setup spi conf\n", __func__);

}
#endif

int gf_spi_write_bytes_ree(struct gf_device *gf_dev, u16 addr,
				u32 data_len, u8 *tx_buf)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	u8 *tmp_buf = NULL;
	u32 package, reminder, retry;

	package = (data_len + 3) / 1024;
	reminder = (data_len + 3) % 1024;

	if ((package > 0) && (reminder != 0)) {
		xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
		retry = 1;
	} else {
		xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
		retry = 0;
	}
	if (xfer == NULL) {
		gf_debug(ERR_LOG, "%s,no memory for SPI transfer\n", __func__);
		return -ENOMEM;
	}
	tmp_buf = gf_dev->spi_buffer;

	/* switch to DMA mode if transfer length larger than 32 bytes */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 3) > 32) {
		gf_dev->spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	spi_message_init(&msg);
	*tmp_buf = 0xF0;
	*(tmp_buf + 1) = (u8)((addr >> 8) & 0xFF);
	*(tmp_buf + 2) = (u8)(addr & 0xFF);
	if (retry) {
		memcpy(tmp_buf + 3, tx_buf, (package * 1024 - 3));
		xfer[0].len = package * 1024;
	} else {
		memcpy(tmp_buf + 3, tx_buf, data_len);
		xfer[0].len = data_len + 3;
	}
	xfer[0].tx_buf = tmp_buf;
#ifdef CONFIG_SPI_MT65XX
	xfer[0].speed_hz = gf_spi_speed;
#endif
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(gf_dev->spi, &msg);

	if (retry) {
		addr = addr + package * 1024 - 3;
		spi_message_init(&msg);
		*tmp_buf = 0xF0;
		*(tmp_buf + 1) = (u8)((addr >> 8) & 0xFF);
		*(tmp_buf + 2) = (u8)(addr & 0xFF);
		memcpy(tmp_buf + 3, (tx_buf + package * 1024 - 3), reminder);
		xfer[1].tx_buf = tmp_buf;
		xfer[1].len = reminder + 3;
		xfer[1].delay_usecs = 5;
#ifdef CONFIG_SPI_MT65XX
		xfer[1].speed_hz = gf_spi_speed;
#endif
		spi_message_add_tail(&xfer[1], &msg);
		spi_sync(gf_dev->spi, &msg);
	}

	/* restore to FIFO mode if has used DMA */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 3) > 32) {
		gf_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}

int gf_spi_read_byte_ree(struct gf_device *gf_dev, u16 addr, u8 *value)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;

	xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
	if (xfer == NULL)
		return -ENOMEM;

	spi_message_init(&msg);
	*gf_dev->spi_buffer = 0xF0;
	*(gf_dev->spi_buffer + 1) = (u8)((addr >> 8) & 0xFF);
	*(gf_dev->spi_buffer + 2) = (u8)(addr & 0xFF);

	xfer[0].tx_buf = gf_dev->spi_buffer;
	xfer[0].len = 3;
#ifdef CONFIG_SPI_MT65XX
	xfer[0].speed_hz = gf_spi_speed;
#endif
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(gf_dev->spi, &msg);

	spi_message_init(&msg);
	/* 4 bytes align */
	*(gf_dev->spi_buffer + 4) = 0xF1;
	xfer[1].tx_buf = gf_dev->spi_buffer + 4;
	xfer[1].rx_buf = gf_dev->spi_buffer + 4;
	xfer[1].len = 2;
#ifdef CONFIG_SPI_MT65XX
	xfer[1].speed_hz = gf_spi_speed;
#endif
	xfer[1].delay_usecs = 5;
	spi_message_add_tail(&xfer[1], &msg);
	spi_sync(gf_dev->spi, &msg);

	*value = *(gf_dev->spi_buffer + 5);

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}


int gf_spi_write_byte_ree(struct gf_device *gf_dev, u16 addr, u8 value)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;

	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	if (xfer == NULL)
		return -ENOMEM;

	spi_message_init(&msg);
	*gf_dev->spi_buffer = 0xF0;
	*(gf_dev->spi_buffer + 1) = (u8)((addr >> 8) & 0xFF);
	*(gf_dev->spi_buffer + 2) = (u8)(addr & 0xFF);
	*(gf_dev->spi_buffer + 3) = value;

	xfer[0].tx_buf = gf_dev->spi_buffer;
	xfer[0].len = 3 + 1;
#ifdef CONFIG_SPI_MT65XX
	xfer[0].speed_hz = gf_spi_speed;
#endif
	xfer[0].delay_usecs = 5;
	spi_message_add_tail(&xfer[0], &msg);
	spi_sync(gf_dev->spi, &msg);

	kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;

	return 0;
}

#endif

#ifdef SUPPORT_REE_OSWEGO
static int gf_check_9p_chip(struct gf_device *gf_dev)
{
	u32 time_out = 0;
	u8 tmp_buf[5] = {0};

	do {
		/* read data start from offset 4 */
		gf_spi_read_bytes_ree(gf_dev, 0x4220, 4, tmp_buf);
		gf_debug(INFO_LOG,
			"%s,9p chip version:0x%x,0x%x,0x%x,0x%x\n", __func__,
			tmp_buf[0], tmp_buf[1], tmp_buf[2], tmp_buf[3]);

		time_out++;
		/* 9P MP chip version is 0x00900802*/
		if ((tmp_buf[3] == 0x00) && (tmp_buf[2] == 0x90)
			&& (tmp_buf[1] == 0x08)) {
			gf_debug(INFO_LOG,
				"%s,9p chip version check pass,time_out=%d\n",
				__func__, time_out);
			return 0;
		}
	} while (time_out < 200);

	gf_debug(INFO_LOG, "%s, 9p chip version read failed, time_out=%d\n",
			__func__, time_out);
	return -1;
}

static int gf_fw_upgrade_prepare(struct gf_device *gf_dev)
{
	u8 tmp_buf[5] = {0};

	gf_spi_write_byte_ree(gf_dev, 0x5081, 0x00);
	/* hold mcu and DSP first */
	gf_spi_write_byte_ree(gf_dev, 0x4180, 0x0c);
	gf_spi_read_bytes_ree(gf_dev, 0x4180, 1, tmp_buf);
	if (tmp_buf[0] == 0x0c) {
		/* 0. enable power supply for DSP and MCU */
		gf_spi_write_byte_ree(gf_dev, 0x4010, 0x0);

		/* 1.Close watch-dog,clear cache enable(write 0 to 0x40B0) */
		gf_spi_write_byte_ree(gf_dev, 0x40B0, 0x00);
		gf_spi_write_byte_ree(gf_dev, 0x404B, 0x00);
	} else {
		gf_debug(ERR_LOG, "%s, Reg = 0x%x, expect 0x0c\n",
			__func__, tmp_buf[4]);
		return -1;
	}

	gf_debug(INFO_LOG, "%s, fw upgrade prepare finished\n", __func__);
	return 0;
}

static int gf_init_flash_fw(struct gf_device *gf_dev)
{
	u8  tmp_buf[11];
	int status = -EINVAL;

#ifndef CPNFIG_SPI_MT65XX
	gf_spi_setup_conf_ree(gf_dev, LOW_SPEED, FIFO_TRANSFER);
#else
	gf_spi_speed = 1*1000000;
#endif

	/*check sensor is goodix, or not*/
	status = gf_check_9p_chip(gf_dev);
	if (status != 0) {
		gf_debug(ERR_LOG, "%s,9p chip version not detect\n", __func__);
		return -ERR_NO_SENSOR;
	}

	mdelay(80);
	memset(tmp_buf, 0x00, 11);
	gf_spi_read_bytes_ree(gf_dev, 0x8000, 10, tmp_buf);
	tmp_buf[6] = '\0';
	gf_debug(INFO_LOG, "[%s],the product id:%s\n", __func__, &tmp_buf[0]);
	gf_debug(INFO_LOG, "[%s],the fw version:0x%x, 0x%x, 0x%x\n",
			__func__, tmp_buf[7], tmp_buf[8], tmp_buf[9]);

	if ((memcmp(&tmp_buf[0], "GFx16M", 6) != 0)
		&& (memcmp(&tmp_buf[0], "GFx18M", 6) != 0)) {
		gf_debug(ERR_LOG,
			"%s, fw version error, need upgrade, reset chip again\n",
			__func__);

		gf_dev->need_update = 1;

		/* reset sensor again */
		gf_miso_gpio_cfg(gf_dev, 1);
		gf_hw_reset(gf_dev, 0);
		udelay(100);
		gf_miso_gpio_cfg(gf_dev, 0);

		memset(tmp_buf, 0x00, 11);
		status = gf_check_9p_chip(gf_dev);
		if (status != 0) {
			gf_debug(ERR_LOG,
				"%s, 9p chip version not detect\n", __func__);
			return -ERR_NO_SENSOR;
		}
		mdelay(10);

		status = gf_fw_upgrade_prepare(gf_dev);
		if (status != 0) {
			gf_debug(ERR_LOG,
				"%s, fw upgrade prepare failed\n", __func__);
			return -ERR_PREPARE_FAIL;
		}
		return -ERR_FW_DESTROY;
	}
	memcpy(id_buf, tmp_buf, 11);
	return 0;
}
#endif
#endif /* SUPPORT_REE_SPI */

#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_MILAN_A
#ifndef CONFIG_TRUSTONIC_TEE_SUPPORT
struct gf_tx_buf_t {
	uint8_t cmd;
	uint8_t addr_h;
	uint8_t addr_l;
	uint8_t len_h;
	uint8_t len_l;
	uint8_t buf[10000];
};

struct gf_rx_buf_t {
	uint8_t cmd;
	uint8_t buf[10000];
};

/* -------------------------------------------------------------------- */
/* normal world SPI read/write function                 */
/* -------------------------------------------------------------------- */
void endian_exchange(u8 *buf, u32 len)
{
	u32 i;
	u8 buf_tmp;
	u32 size = len / 2;

	for (i = 0; i < size; i++) {
		buf_tmp = buf[2 * i + 1];
		buf[2 * i + 1] = buf[2 * i];
		buf[2 * i] = buf_tmp;
	}
}

int gf_spi_read_bytes_ree_new(struct gf_device *gf_dev, u16 addr,
					u32 data_len, u8 *buf)
{
	struct spi_message msg;
	struct spi_transfer xfer;

	struct gf_tx_buf_t *g_tx_buf;
	struct gf_rx_buf_t *g_rx_buf;

	g_tx_buf = kzalloc(10000 + 5, GFP_KERNEL);
	g_rx_buf = kzalloc(10000 + 5, GFP_KERNEL);

	/* gf_debug(INFO_LOG,
	 * %s %d g_tx_buf:%p g_rx_buf:%p\n",
	 * __func__, __LINE__, g_tx_buf, g_rx_buf);
	 */

	g_tx_buf->cmd = 0xF0;
	g_tx_buf->addr_h = (uint8_t) ((addr >> 8) & 0xFF);
	g_tx_buf->addr_l = (uint8_t) (addr & 0xFF);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.tx_buf = g_tx_buf;
	xfer.rx_buf = g_rx_buf;
	xfer.len = 3;
#ifdef CONFIG_SPI_MT65XX
	xfer.speed_hz = gf_spi_speed;
#endif

	spi_message_add_tail(&xfer, &msg);
	spi_sync(gf_dev->spi, &msg);

	/* switch to DMA mode if transfer length larger than 32 bytes */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 1) > 32) {
		gf_dev->spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(struct spi_transfer));
	g_tx_buf->cmd = 0xF1;

	xfer.tx_buf = g_tx_buf;
	xfer.rx_buf = g_rx_buf;

	xfer.len = data_len + 1;
#ifdef CONFIG_SPI_MT65XX
	xfer.speed_hz = gf_spi_speed;
#endif

	spi_message_add_tail(&xfer, &msg);
	spi_sync(gf_dev->spi, &msg);

	memcpy(buf, g_rx_buf->buf, data_len);

#ifdef SUPPORT_REE_MILAN_A
	/*change the read data to little endian. */
	endian_exchange(buf, data_len);
#endif
	/* restore to FIFO mode if has used DMA */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 1) > 32) {
		gf_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	kfree(g_tx_buf);
	kfree(g_rx_buf);

	return 0;
}

int gf_spi_write_bytes_ree_new(struct gf_device *gf_dev, u16 addr,
					u32 data_len, u8 *buf)
{
	struct spi_message msg;
	struct spi_transfer xfer;

	struct gf_tx_buf_t *g_tx_buf;
	struct gf_rx_buf_t *g_rx_buf;

	g_tx_buf = kzalloc(10000 + 5, GFP_KERNEL);
	g_rx_buf = kzalloc(10000 + 5, GFP_KERNEL);

	/* gf_log(GF_INFO,
	 * "%s %d g_tx_buf:%p g_rx_buf:%p\n",
	 * __func__, __LINE__, g_tx_buf, g_rx_buf);
	 */

	g_tx_buf->cmd = 0xF0;
	g_tx_buf->addr_h = (uint8_t) ((addr >> 8) & 0xFF);
	g_tx_buf->addr_l = (uint8_t) (addr & 0xFF);

#ifdef SUPPORT_REE_MILAN_A
	g_tx_buf->len_h = (uint8_t) (((data_len / 2) >> 8) & 0xFF);
	g_tx_buf->len_l = (uint8_t) ((data_len / 2) & 0xFF);
#endif

	if (buf != NULL) {
		memcpy(g_tx_buf->buf, buf, data_len);
#ifdef SUPPORT_REE_MILAN_A
		/* change the read data to little endian. */
		endian_exchange(g_tx_buf->buf, data_len);
#endif
	}

	/* switch to DMA mode if transfer length larger than 32 bytes */
#ifndef CONFIG_SPI_MT65XX
	if ((data_len + 5) > 32) {
		gf_dev->spi_mcc.com_mod = DMA_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.tx_buf = g_tx_buf;
	xfer.rx_buf = g_rx_buf;
	xfer.len = data_len + 5;
#ifdef CONFIG_SPI_MT65XX
	xfer.speed_hz = gf_spi_speed;
#endif

	spi_message_add_tail(&xfer, &msg);
	spi_sync(gf_dev->spi, &msg);
#ifndef CONFIG_SPI_MT65XX
	/* restore to FIFO mode if has used DMA */
	if ((data_len + 3) > 32) {
		gf_dev->spi_mcc.com_mod = FIFO_TRANSFER;
		spi_setup(gf_dev->spi);
	}
#endif

	kfree(g_tx_buf);
	kfree(g_rx_buf);

	return 0;
}

int gf_milan_a_series_init_process(struct gf_device *gf_dev)
{
	u32 fw_len = 5120;
	u32 loop_time = 0;
	u32 i = 0;
	u16 value;
	u8 cmp_buf[1024];

	FUNC_ENTRY();

	/* gf_hw_reset(gf_dev, 0); */
	memset(cmp_buf, 0, 1024);

	value = 0x0200;
	gf_spi_write_bytes_ree_new(gf_dev, 0x014E, 2, (u8 *) &value);
	value = 0x0003;
	gf_spi_write_bytes_ree_new(gf_dev, 0x0146, 2, (u8 *) &value);

	gf_spi_write_bytes_ree_new(gf_dev, 0x0842,
					sizeof(gf_cfg)/sizeof(u8), gf_cfg);

	gf_spi_read_bytes_ree_new(gf_dev, 0x0842,
					sizeof(gf_cfg)/sizeof(u8), cmp_buf);

	if (strncmp(cmp_buf, gf_cfg, sizeof(gf_cfg)/sizeof(u8)) != 0)
		gf_debug(INFO_LOG, "[%s],download cfg failed.\n", __func__);
	else
		gf_debug(INFO_LOG, "[%s],%d download cfg success.\n",
				__func__, __LINE__);

	loop_time = fw_len/1000;

	for (i = 0; i < loop_time; i++)
		gf_spi_write_bytes_ree_new(gf_dev, (0x2000 + i * 1000), 1000,
						(gf_fw + i * 1000));

	gf_spi_write_bytes_ree_new(gf_dev, 0x2000 + loop_time * 1000,
			fw_len%1000, gf_fw + loop_time * 1000);

	for (i = 0; i < loop_time; i++) {
		gf_spi_read_bytes_ree_new(gf_dev, 0x2000 + i * 1000,
						1000, cmp_buf);
		if (strncmp(cmp_buf, gf_fw + i * 1000, 1000) != 0)
			gf_debug(INFO_LOG, "[%s],download fw failed[%d]\n",
					__func__, i);
		else
			gf_debug(INFO_LOG, "[%s],%d download fw OK[%d]\n",
					__func__, __LINE__, i);
	}

	gf_spi_read_bytes_ree_new(gf_dev, 0x2000 + loop_time * 1000,
					fw_len%1000, cmp_buf);

	if (strncmp(cmp_buf, gf_fw + loop_time * 1000, fw_len%1000) != 0)
		gf_debug(INFO_LOG, "[%s],download fw failed.\n", __func__);
	else
		gf_debug(INFO_LOG, "[%s],%d download fw success.\n",
				__func__, __LINE__);

	value = 0x0000;
	gf_spi_write_bytes_ree_new(gf_dev, 0x0146, 2, (u8 *) &value);
	value = 0x0000;
	gf_spi_write_bytes_ree_new(gf_dev, 0x014E, 2, (u8 *) &value);

	mdelay(10);

	gf_spi_read_bytes_ree_new(gf_dev, 0x0836, 2, (u8 *) &value);
	gf_debug(INFO_LOG, "[%s], fw irq read == 0x%X.\n", __func__, value);

	value = 0x0000;
	gf_spi_write_bytes_ree_new(gf_dev, 0x0836, 2, (u8 *) &value);

	value = 0x0002;
	gf_spi_write_bytes_ree_new(gf_dev, 0x0834, 2, (u8 *) &value);

	gf_spi_read_bytes_ree_new(gf_dev, 0x0834, 2, (u8 *) &value);
	gf_debug(INFO_LOG, "[%s], set mode read = 0x%X\n", __func__, value);
	FUNC_EXIT();

	return 0;
}

#endif  /* CONFIG_TRUSTONIC_TEE_SUPPORT */
#endif /* SUPPORT_REE_MILAN_A */
#endif /* SUPPORT_REE_SPI */

static const struct file_operations gf_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.	It'll simplify things
	 * too, except for the locking.
	 */
	.write =	gf_write,
	.read =		gf_read,
	.unlocked_ioctl = gf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gf_compat_ioctl,
#endif
	.open =		gf_open,
	.release =	gf_release,
	.poll	= gf_poll,
};

/*----------------------------------------------------------------------*/

static int gf_probe(struct spi_device *spi)
{
	struct gf_device *gf_dev = NULL;
	int status = -EINVAL;
#ifdef SUPPORT_REE_MILAN_A
	u8 tmp_buf[2] = {0};
#endif

	FUNC_ENTRY();

	/* Allocate driver data */
	gf_dev = kzalloc(sizeof(struct gf_device), GFP_KERNEL);
	if (!gf_dev) {
		status = -ENOMEM;
		goto err;
	}

	spin_lock_init(&gf_dev->spi_lock);
	mutex_init(&gf_dev->buf_lock);
	mutex_init(&gf_dev->release_lock);

	INIT_LIST_HEAD(&gf_dev->device_entry);

	gf_dev->device_count     = 0;
	gf_dev->probe_finish     = 0;
	gf_dev->system_status    = 0;
	gf_dev->need_update      = 0;

	/*setup gf configurations.*/
	gf_debug(INFO_LOG, "%s, Setting gf device configuration\n", __func__);

	/* Initialize the driver data */
	gf_dev->spi = spi;

	/* setup SPI parameters */
	/* CPOL=CPHA=0, speed 1MHz */
	gf_dev->spi->mode            = SPI_MODE_0;
	gf_dev->spi->bits_per_word   = 8;
	gf_dev->spi->max_speed_hz    = 1 * 1000 * 1000;
#ifndef CONFIG_SPI_MT65XX
	memcpy(&gf_dev->spi_mcc, &spi_ctrldata, sizeof(struct mt_chip_conf));
	gf_dev->spi->controller_data = (void *)&gf_dev->spi_mcc;
	gf_debug(INFO_LOG, "%s %d,Old SPI,need to spi_setup()\n",
			__func__, __LINE__);
	spi_setup(gf_dev->spi);
#endif
	gf_dev->irq = 0;
	spi_set_drvdata(spi, gf_dev);

	/* allocate buffer for SPI transfer */
	gf_dev->spi_buffer = kzalloc(bufsiz, GFP_KERNEL);
	if (gf_dev->spi_buffer == NULL) {
		status = -ENOMEM;
		goto err_buf;
	}

	/* get gpio info from dts or defination */
	gf_get_gpio_dts_info(gf_dev);
	gf_get_sensor_dts_info();

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
	if (rst_mt6306_support == 1 && rst_mt6306_gpio != -1)
		mt6306_set_gpio_dir(rst_mt6306_gpio, MT6306_GPIO_DIR_OUT);

	if (rst_mt6306_support == 1 && rst_mt6306_gpio == -1)
		goto err_class;
#endif

	/*enable the power*/
	gf_hw_power_enable(gf_dev, 1);
	gf_bypass_flash_gpio_cfg();

	/* delete spi clk handle because SPI will enable clk default  */
	/* gf_spi_clk_enable(gf_dev, 1); */

	/* check firmware Integrity */
	gf_debug(INFO_LOG, "%s, Sensor type : %s\n",
			__func__, CONFIG_GOODIX_SENSOR_TYPE);

#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_OSWEGO
	{
		int i = 0;
		int sensor_num = 0;

		sensor_num = ARRAY_SIZE(oswego_m_sensor_type);
		for (i = 0; i < sensor_num; i++) {
			if (strncmp(CONFIG_GOODIX_SENSOR_TYPE,
				oswego_m_sensor_type[i],
				strlen(oswego_m_sensor_type[i])) == 0) {
				/* put miso high to select SPI transfer */
				gf_miso_gpio_cfg(gf_dev, 1);
				gf_hw_reset(gf_dev, 0);
				udelay(100);
				gf_miso_gpio_cfg(gf_dev, 0);

				status = gf_init_flash_fw(gf_dev);
				if (status == -ERR_NO_SENSOR) {
					gf_debug(ERR_LOG,
						"%s, no goodix sensor\n",
						__func__);
					goto err_fw;
				}
				break;
			}
		}
	}
#endif
#endif /* SUPPORT_REE_SPI */

/* make fingerprint to lower power mode for nonTEE project */
#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_MILAN_A
#ifndef CONFIG_TRUSTONIC_TEE_SUPPORT
	{
	u8 id_tmp[2] = {0};
	u16 chip_id;

	gf_spi_read_bytes_ree_new(gf_dev, 0x0142, 2, id_tmp);
	chip_id = (u16)id_tmp[0];
	chip_id += ((u16)id_tmp[1]) << 8;
	gf_debug(INFO_LOG, "[%s],id_tmp[0]0x%x id_tmp[1]=0x%x chip_id:0x%x\n",
	__func__, id_tmp[0], id_tmp[1], chip_id);

	if (0x12A4 == chip_id || 0x12A1 == chip_id) {
		gf_debug(INFO_LOG,
			"[%s][%d]no TEE support so make the sensor sleep\n",
			__func__, __LINE__);
		gf_milan_a_series_init_process(gf_dev);
	}

	}
#endif
#endif
#endif /* SUPPORT_REE_SPI */
	/* create class */
	gf_dev->class = class_create(THIS_MODULE, GF_CLASS_NAME);
	if (IS_ERR(gf_dev->class)) {
		gf_debug(ERR_LOG, "%s, Failed to create class.\n", __func__);
		status = -ENODEV;
		goto err_class;
	}

	/* get device no */
	if (GF_DEV_MAJOR > 0) {
		gf_dev->devno = MKDEV(GF_DEV_MAJOR, gf_dev->device_count++);
		status = register_chrdev_region(gf_dev->devno, 1, GF_DEV_NAME);
	} else {
		status = alloc_chrdev_region(&gf_dev->devno,
				gf_dev->device_count++, 1, GF_DEV_NAME);
	}
	if (status < 0) {
		gf_debug(ERR_LOG, "%s, Failed to alloc devno.\n", __func__);
		goto err_devno;
	} else {
		gf_debug(INFO_LOG, "%s, major=%d, minor=%d\n", __func__,
				MAJOR(gf_dev->devno), MINOR(gf_dev->devno));
	}

	/* create device */
	gf_dev->device = device_create(gf_dev->class, &spi->dev,
					gf_dev->devno, gf_dev, GF_DEV_NAME);
	if (IS_ERR(gf_dev->device)) {
		gf_debug(ERR_LOG, "%s, Failed to create device.\n", __func__);
		status = -ENODEV;
		goto err_device;
	} else {
		mutex_lock(&device_list_lock);
		list_add(&gf_dev->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
		gf_debug(INFO_LOG, "%s, device create success.\n", __func__);
	}

	/* create sysfs */
	status = sysfs_create_group(&spi->dev.kobj, &gf_debug_attr_group);
	if (status) {
		gf_debug(ERR_LOG, "%s,create sysfs file failed\n", __func__);
		status = -ENODEV;
		goto err_sysfs;
	} else {
		gf_debug(INFO_LOG, "%s,create sysfs file OK\n", __func__);
	}

	/* cdev init and add */
	cdev_init(&gf_dev->cdev, &gf_fops);
	gf_dev->cdev.owner = THIS_MODULE;
	status = cdev_add(&gf_dev->cdev, gf_dev->devno, 1);
	if (status) {
		gf_debug(ERR_LOG, "%s, Failed to add cdev.\n", __func__);
		goto err_cdev;
	}

	/*register device within input system.*/
	gf_dev->input = input_allocate_device();
	if (gf_dev->input == NULL) {
		gf_debug(ERR_LOG, "%s, Failed to allocate input device.\n",
			__func__);
		status = -ENOMEM;
		goto err_input;
	}

	__set_bit(EV_KEY, gf_dev->input->evbit);
	__set_bit(GF_KEY_INPUT_HOME, gf_dev->input->keybit);

	__set_bit(GF_KEY_INPUT_MENU, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_BACK, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_POWER, gf_dev->input->keybit);

	__set_bit(GF_NAV_INPUT_UP, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_DOWN, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_RIGHT, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_LEFT, gf_dev->input->keybit);
	__set_bit(GF_KEY_INPUT_CAMERA, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_CLICK, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_DOUBLE_CLICK, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_LONG_PRESS, gf_dev->input->keybit);
	__set_bit(GF_NAV_INPUT_HEAVY, gf_dev->input->keybit);

	gf_dev->input->name = GF_INPUT_NAME;
	if (input_register_device(gf_dev->input)) {
		gf_debug(ERR_LOG,
			"%s, Failed to register input device\n", __func__);
		status = -ENODEV;
		goto err_input_2;
	}

	/* wakeup source init */
	wakeup_source_init(&fp_wakeup_source, "fingerprint wakelock");

	/* netlink interface init */
	status = gf_netlink_init(gf_dev);
	if (status == -1) {
		mutex_lock(&gf_dev->release_lock);
		input_unregister_device(gf_dev->input);
		gf_dev->input = NULL;
		mutex_unlock(&gf_dev->release_lock);
		goto err_input;
	}

	gf_dev->probe_finish = 1;
	gf_dev->is_sleep_mode = 0;
	gf_debug(INFO_LOG, "%s probe finished\n", __func__);

#ifdef SUPPORT_REE_MILAN_A
	gf_spi_read_bytes_ree(gf_dev, 0x0142, 2, tmp_buf);
	gf_debug(INFO_LOG, "%s line:%d ChipID:0x%x  0x%x\n",
			__func__, __LINE__, tmp_buf[0], tmp_buf[1]);
	memcpy(id_buf, tmp_buf, 2);
#endif
	/* delete spi clk handle because SPI will enable clk default */
	/* gf_spi_clk_enable(gf_dev, 0); */

	FUNC_EXIT();
	return 0;

err_input_2:
	mutex_lock(&gf_dev->release_lock);
	input_free_device(gf_dev->input);
	gf_dev->input = NULL;
	mutex_unlock(&gf_dev->release_lock);

err_input:
	cdev_del(&gf_dev->cdev);

err_cdev:
	sysfs_remove_group(&spi->dev.kobj, &gf_debug_attr_group);

err_sysfs:
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);

err_device:
	unregister_chrdev_region(gf_dev->devno, 1);

err_devno:
	class_destroy(gf_dev->class);

err_class:
#ifdef SUPPORT_REE_SPI
#ifdef SUPPORT_REE_OSWEGO
err_fw:
#endif
#endif
	gf_hw_power_enable(gf_dev, 0);
	gf_spi_clk_enable(gf_dev, 0);
	kfree(gf_dev->spi_buffer);
err_buf:
	mutex_destroy(&gf_dev->buf_lock);
	mutex_destroy(&gf_dev->release_lock);
	spi_set_drvdata(spi, NULL);
	gf_dev->spi = NULL;
	kfree(gf_dev);
	gf_dev = NULL;
err:

	FUNC_EXIT();
	return status;
}

static int gf_remove(struct spi_device *spi)
{
	struct gf_device *gf_dev = spi_get_drvdata(spi);

	FUNC_ENTRY();

	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq) {
		free_irq(gf_dev->irq, gf_dev);
		gf_dev->irq_count = 0;
		gf_dev->irq = 0;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (gf_dev->early_suspend.suspend)
		unregister_early_suspend(&gf_dev->early_suspend);
#else
	fb_unregister_client(&gf_dev->notifier);
#endif

	mutex_lock(&gf_dev->release_lock);
	if (gf_dev->input == NULL) {
		mutex_unlock(&gf_dev->release_lock);
		kfree(gf_dev);
		FUNC_EXIT();
		return 0;
	}
	input_unregister_device(gf_dev->input);
	gf_dev->input = NULL;
	mutex_unlock(&gf_dev->release_lock);

	mutex_lock(&gf_dev->release_lock);
	if (gf_dev->spi_buffer != NULL) {
		kfree(gf_dev->spi_buffer);
		gf_dev->spi_buffer = NULL;
	}
	mutex_unlock(&gf_dev->release_lock);

	gf_netlink_destroy(gf_dev);
	cdev_del(&gf_dev->cdev);
	sysfs_remove_group(&spi->dev.kobj, &gf_debug_attr_group);
	device_destroy(gf_dev->class, gf_dev->devno);
	list_del(&gf_dev->device_entry);

	unregister_chrdev_region(gf_dev->devno, 1);
	class_destroy(gf_dev->class);
	gf_hw_power_enable(gf_dev, 0);
	gf_spi_clk_enable(gf_dev, 0);

	spin_lock_irq(&gf_dev->spi_lock);
	spi_set_drvdata(spi, NULL);
	gf_dev->spi = NULL;
	spin_unlock_irq(&gf_dev->spi_lock);

	mutex_destroy(&gf_dev->buf_lock);
	mutex_destroy(&gf_dev->release_lock);

	kfree(gf_dev);
	FUNC_EXIT();
	return 0;
}

/*--------------------------------------------------------------------*/
static struct spi_driver gf_spi_driver = {
	.driver = {
		.name = GF_DEV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gf_of_match,
#endif
	},
	.probe = gf_probe,
	.remove = gf_remove,
};

static int __init gf_init(void)
{
	int status = 0;

	FUNC_ENTRY();

	status = spi_register_driver(&gf_spi_driver);
	if (status < 0) {
		gf_debug(ERR_LOG,
			"%s, Failed to register SPI driver.\n", __func__);
		return -EINVAL;
	}

	FUNC_EXIT();
	return status;
}
/* module_init(gf_init); */
late_initcall(gf_init);

static void __exit gf_exit(void)
{
	FUNC_ENTRY();
	spi_unregister_driver(&gf_spi_driver);
	FUNC_EXIT();
}
module_exit(gf_exit);


MODULE_AUTHOR("goodix");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:gf_spi");
