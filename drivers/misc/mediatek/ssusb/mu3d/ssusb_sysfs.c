/*
 * MTK SSUSB driver sysfs support
 *
 * Copyright 2015 MediaTek
 * Author: chunfeng.yun <chunfeng.yun@mediatek.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>

#include "musb_core.h"
#include "ssusb_sysfs.h"
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"
#include "ssusb_io.h"
#include "xhci-mtk.h"


#define REGS_LIMIT_XHCI 0x1000
#define REGS_LIMIT_MU3D 0x3000
#define REGS_LIMIT_PHYS 0x12000

unsigned int cable_mode = CABLE_MODE_NORMAL;

/*--FOR INSTANT POWER ON USAGE--------------------------------------------------*/


const char *const usb_mode_str[CABLE_MODE_MAX] = { "CHRG_ONLY", "NORMAL", "HOST_ONLY" };

ssize_t musb_cmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!dev) {
		mu3d_dbg(K_ERR, "dev is null!!\n");
		return 0;
	}
	return sprintf(buf, "%s\n", usb_mode_str[cable_mode]);
}

ssize_t musb_cmode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_to_ssusb(dev);
	struct musb *musb = ssusb->mu3di;
	unsigned int cmode;

	if (!dev) {
		mu3d_dbg(K_ERR, "dev is null!!\n");
		return count;
	} else if (1 == kstrtoint(buf, 0, &cmode)) {
		mu3d_dbg(K_INFO, "%s %s --> %s\n", __func__, usb_mode_str[cable_mode],
			 usb_mode_str[cmode]);

		if (cmode >= CABLE_MODE_MAX)
			cmode = CABLE_MODE_NORMAL;

		if (cable_mode != cmode) {
			if (cmode == CABLE_MODE_CHRG_ONLY) {	/* IPO shutdown, disable USB */
				if (musb) {
					musb->usb_mode = CABLE_MODE_CHRG_ONLY;
					mt_usb_disconnect();
				}
			} else if (cmode == CABLE_MODE_HOST_ONLY) {
				if (musb) {
					musb->usb_mode = CABLE_MODE_HOST_ONLY;
					mt_usb_disconnect();
				}
			} else {	/* IPO bootup, enable USB */
				if (musb) {
					musb->usb_mode = CABLE_MODE_NORMAL;
					mt_usb_connect();
				}
			}
			cable_mode = cmode;
		}
	}
	return count;
}


static ssize_t musb_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ssusb_mtk *ssusb = dev_to_ssusb(dev);
	struct musb *musb = ssusb->mu3di;
	unsigned long flags;
	int ret = -EINVAL;

	/* TODO : judge when support DRD, other wise, shouldn't call mtk_is_host_mode() */
	spin_lock_irqsave(&musb->lock, flags);
	ret = sprintf(buf, "current: %s\n (echo 0: to device, 1: to host)\n",
		      mtk_is_host_mode() ? "host" : "device");
	spin_unlock_irqrestore(&musb->lock, flags);

	return ret;
}

ssize_t musb_mode_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_to_ssusb(dev);
	struct musb *musb = ssusb->mu3di;
	int old_mode = !!mtk_is_host_mode();
	int mode;

	if (!dev) {
		mu3d_dbg(K_ERR, "dev is null!!\n");
		return count;
	} else if (1 == kstrtoint(buf, 0, &mode)) {
		mu3d_dbg(K_INFO, "%s() --> %s\n", __func__, mode ? "host" : "device");

		mode = !!mode;
		if (old_mode == mode) {
			mu3d_dbg(K_INFO, "already in %s\n", mode ? "host" : "device");
			return count;
		}
		if (mode) {
			musb_stop(musb);
			msleep(200);	/* if not, there is something wrong with xhci's port status */
			ssusb_mode_switch_manual(ssusb, 1);
		} else {
			ssusb_mode_switch_manual(ssusb, 0);
			msleep(100);
			musb_start(musb);
		}

	} else {
		mu3d_dbg(K_INFO, "echo 0 > mode: to deice, echo 1 > mode: to host\n");
	}
	return count;
}

static DEVICE_ATTR(mode, 0664, musb_mode_show, musb_mode_store);

static ssize_t ssusb_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* struct ssusb_mtk *ssusb = dev_to_ssusb(dev); */
	int ret = -EINVAL;


	ret = sprintf(buf, "SSUSB register operation interface help info.\n"
		      "  rx - read xhci mac register: offset [len]\n"
		      "  wx - write xhci mac register: offset value\n"
		      "  rm - read mu3d mac register: offset [len]\n"
		      "  wm - write mu3d mac register: offset value\n"
		      "  rp - read phy register: offset [len]\n"
		      "  wp - write phy register: offset value\n"
		      "  NOTE: numbers should be HEX\n");

	return ret;
}


static void ssusb_write_reg(struct ssusb_mtk *ssusb, const char *buf)
{
	struct musb *musb = ssusb->mu3di;
	void __iomem *base;
	u32 offset = 0;
	u32 value = 0;
	u32 old_val = 0;
	u32 limit = 0;
	u32 param;

	param = sscanf(buf, "%*s 0x%x 0x%x", &offset, &value);
	mu3d_dbg(K_INFO, "params-%d (offset: %#x, value: %#x)\n", param, offset, value);

	switch (buf[1]) {
	case 'x':
		base = get_xhci_base();
		limit = REGS_LIMIT_XHCI;
		mu3d_dbg(K_INFO, "write xhci's reg:\n");
		break;
	case 'm':
		base = musb->mac_base;
		limit = REGS_LIMIT_MU3D;
		mu3d_dbg(K_INFO, "write mu3d's reg:\n");
		break;
	case 'p':
		base = musb->sif_base;
		limit = REGS_LIMIT_PHYS;
		mu3d_dbg(K_INFO, "write sif's reg:\n");
		break;
	default:
		base = NULL;
	}
	if (!base || (param != 2)) {
		mu3d_dbg(K_ERR, "params are invalid!\n");
		return;
	}

	offset &= ~0x3;		/* 4-bytes align */
	if (offset >= limit) {
		mu3d_dbg(K_ERR, "reg's offset overrun!\n");
		return;
	}
	old_val = mu3d_readl(base, offset);
	mu3d_writel(base, offset, value);
	mu3d_dbg(K_INFO, "0x%8.8x : 0x%8.8x --> 0x%8.8x\n", offset, old_val,
		 mu3d_readl(base, offset));
}

static void read_single_reg(void __iomem *base, u32 offset, u32 limit)
{
	u32 value;

	offset &= ~0x3;		/* 4-bytes align */
	if (offset >= limit) {
		mu3d_dbg(K_ERR, "reg's offset overrun!\n");
		return;
	}
	value = mu3d_readl(base, offset);
	mu3d_dbg(K_INFO, "0x%8.8x : 0x%8.8x\n", offset, value);
}

static void read_multi_regs(void __iomem *base, u32 offset, u32 len, u32 limit)
{
	int i;

	/* at least 4 ints */
	offset &= ~0xF;
	len = (len + 0x3) & ~0x3;

	if (offset + len > limit) {
		mu3d_dbg(K_ERR, "reg's offset overrun!\n");
		return;
	}

	len >>= 2;
	mu3d_dbg(K_INFO, "read regs [%#x, %#x)\n", offset, offset + (len << 4));
	for (i = 0; i < len; i++) {
		mu3d_dbg(K_INFO, "0x%8.8x : 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n",
			 offset, mu3d_readl(base, offset), mu3d_readl(base, offset + 0x4),
			 mu3d_readl(base, offset + 0x8), mu3d_readl(base, offset + 0xc));
		offset += 0x10;
	}
}

static void ssusb_read_regs(struct ssusb_mtk *ssusb, const char *buf)
{
	struct musb *musb = ssusb->mu3di;
	void __iomem *base;
	u32 offset = 0;
	u32 len = 0;
	u32 limit = 0;
	u32 param;

	param = sscanf(buf, "%*s 0x%x 0x%x", &offset, &len);
	mu3d_dbg(K_INFO, "params-%d (offset: %#x, len: %#x)\n", param, offset, len);

	switch (buf[1]) {
	case 'x':
		base = get_xhci_base();
		limit = REGS_LIMIT_XHCI;
		mu3d_dbg(K_INFO, "read xhci's reg:\n");
		break;
	case 'm':
		base = musb->mac_base;
		limit = REGS_LIMIT_MU3D;
		mu3d_dbg(K_INFO, "read mu3d's reg:\n");
		break;
	case 'p':
		base = musb->sif_base;
		limit = REGS_LIMIT_PHYS;
		mu3d_dbg(K_INFO, "read sif's reg:\n");
		break;
	default:
		base = NULL;
	}
	if (!base || !param) {
		mu3d_dbg(K_ERR, "params are invalid!\n");
		return;
	}

	if (1 == param)
		read_single_reg(base, offset, limit);
	else
		read_multi_regs(base, offset, len, limit);
}

static ssize_t
ssusb_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	struct ssusb_mtk *ssusb = dev_to_ssusb(dev);

	mu3d_dbg(K_INFO, " cmd: %s\n", buf);
	if (!ssusb->is_power_on) {
		mu3d_dbg(K_INFO, "power off, so can't access regs\n");
		goto out;
	}

	switch (buf[0]) {
	case 'w':
		ssusb_write_reg(ssusb, buf);
		break;
	case 'r':
		ssusb_read_regs(ssusb, buf);
		break;
	default:
		mu3d_dbg(K_INFO, "No such cmd\n");
	}

out:
	return n;
}

static DEVICE_ATTR(reg, 0664, ssusb_reg_show, ssusb_reg_store);



DEVICE_ATTR(cmode, 0664, musb_cmode_show, musb_cmode_store);

static struct attribute *musb_attributes[] = {
	&dev_attr_mode.attr,
	&dev_attr_reg.attr,
	&dev_attr_cmode.attr,
	NULL
};

static const struct attribute_group musb_attr_group = {
	.attrs = musb_attributes,
};

int ssusb_sysfs_init(struct musb *musb)
{
	return sysfs_create_group(&musb->controller->kobj, &musb_attr_group);
}

void ssusb_sysfs_exit(struct musb *musb)
{
	sysfs_remove_group(&musb->controller->kobj, &musb_attr_group);
}
