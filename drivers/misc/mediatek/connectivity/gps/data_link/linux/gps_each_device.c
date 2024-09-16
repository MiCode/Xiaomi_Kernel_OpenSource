/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "gps_each_device.h"
#include "gps_each_link.h"
#include "gps_dsp_fsm.h"
#if GPS_DL_MOCK_HAL
#include "gps_mock_mvcd.h"
#endif
#include "gps_dl_hal.h"
#include "gps_dl_ctrld.h"
#include "gps_data_link_devices.h"
#include "gps_dl_subsys_reset.h"
#include "gps_dl_hist_rec.h"

static ssize_t gps_each_device_read(struct file *filp,
	char __user *buf, size_t count, loff_t *f_pos)
{
	int retval;
	int retlen;
	int pid;
	struct gps_each_device *dev;
	enum gps_dl_link_id_enum link_id;
	bool print_log = false;

	dev = (struct gps_each_device *)filp->private_data;
	pid = current->pid;
	link_id = (enum gps_dl_link_id_enum)dev->index;

	/* show read log after ram code downloading to avoid print too much */
	if (gps_dsp_state_get(link_id) != GPS_DSP_ST_RESET_DONE) {
		GDL_LOGXI_DRW(link_id, "buf_len = %ld, pid = %d", count, pid);
		print_log = true;
	} else {
		GDL_LOGXD_DRW(link_id, "buf_len = %ld, pid = %d", count, pid);
	}
	gps_each_link_rec_read(link_id, pid, count, DRW_ENTER);

	retlen = gps_each_link_read(link_id, &dev->i_buf[0], count);
	if (retlen > 0) {
		retval = copy_to_user(buf, &dev->i_buf[0], retlen);
		if (retval != 0) {
			GDL_LOGXW_DRW(link_id, "copy to user len = %d, retval = %d",
				retlen, retval);
			retlen = -EFAULT;
		}
	}
	if (print_log)
		GDL_LOGXI_DRW(link_id, "ret_len = %d", retlen);
	else
		GDL_LOGXD_DRW(link_id, "ret_len = %d", retlen);
	gps_each_link_rec_read(link_id, pid, retlen, DRW_RETURN);
	return retlen;
}

static ssize_t gps_each_device_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *f_pos)
{
	int retval;
	int retlen = 0;
	int copy_size;
	int pid;
	struct gps_each_device *dev;
	enum gps_dl_link_id_enum link_id;
	bool print_log = false;

	dev = (struct gps_each_device *)filp->private_data;
	pid = current->pid;
	link_id = (enum gps_dl_link_id_enum)dev->index;

	/* show write log after ram code downloading to avoid print too much */
	if (gps_dsp_state_get(link_id) != GPS_DSP_ST_RESET_DONE) {
		GDL_LOGXI_DRW(link_id, "len = %ld, pid = %d", count, pid);
		print_log = true;
	} else {
		GDL_LOGXD_DRW(link_id, "len = %ld, pid = %d", count, pid);
	}
	gps_each_link_rec_write(link_id, pid, count, DRW_ENTER);

	if (count > 0) {
		if (count > GPS_DATA_PATH_BUF_MAX) {
			GDL_LOGXW_DRW(link_id, "len = %ld is too long", count);
			copy_size = GPS_DATA_PATH_BUF_MAX;
		} else
			copy_size = count;

		retval = copy_from_user(&dev->o_buf[0], &buf[0], copy_size);
		if (retval != 0) {
			GDL_LOGXW_DRW(link_id, "copy from user len = %d, retval = %d",
				copy_size, retval);
			retlen = -EFAULT;
		} else {
			retval = gps_each_link_write(link_id, &dev->o_buf[0], copy_size);
			if (retval == 0)
				retlen = copy_size;
			else
				retlen = 0;
		}
	}

	if (print_log)
		GDL_LOGXI_DRW(link_id, "ret_len = %d", retlen);
	else
		GDL_LOGXD_DRW(link_id, "ret_len = %d", retlen);
	gps_each_link_rec_write(link_id, pid, retlen, DRW_RETURN);
	return retlen;
}

#if 0
void gps_each_device_data_submit(unsigned char *buf, unsigned int len, int index)
{
	struct gps_each_device *dev;

	dev = gps_dl_device_get(index);

	GDL_LOGI("gps_each_device_data_submit len = %d, index = %d, dev = %p",
		len, index, dev);

	if (!dev)
		return;

	if (!dev->is_open)
		return;

#if GPS_DL_CTRLD_MOCK_LINK_LAYER
	/* TODO: using mutex, len check */
	memcpy(&dev->i_buf[0], buf, len);
	dev->i_len = len;
	wake_up(&dev->r_wq);
#else
	gps_dl_add_to_rx_queue(buf, len, index);
	/* wake_up(&dev->r_wq); */
#endif

	GDL_LOGI("gps_each_device_data_submit copy and wakeup done");
}
#endif

static int gps_each_device_open(struct inode *inode, struct file *filp)
{
	struct gps_each_device *dev; /* device information */
	int retval;

	dev = container_of(inode->i_cdev, struct gps_each_device, cdev);
	filp->private_data = dev; /* for other methods */

	GDL_LOGXW(dev->index, "major = %d, minor = %d, pid = %d",
		imajor(inode), iminor(inode), current->pid);

	if (!dev->is_open) {
		retval = gps_each_link_open((enum gps_dl_link_id_enum)dev->index);

		if (0 == retval) {
			dev->is_open = true;
			gps_each_link_rec_reset(dev->index);
		} else
			return retval;
	}

	return 0;
}

static int gps_each_device_hw_resume(enum gps_dl_link_id_enum link_id)
{
	int pid;
	int retval;

	pid = current->pid;
	GDL_LOGXW(link_id, "pid = %d", pid);

	retval = gps_each_link_hw_resume(link_id);

	/* device read may arrive before resume, not reset the recording here
	 * gps_each_link_rec_reset(link_id);
	 */

	return retval;
}

static int gps_each_device_release(struct inode *inode, struct file *filp)
{
	struct gps_each_device *dev;

	dev = (struct gps_each_device *)filp->private_data;
	dev->is_open = false;

	GDL_LOGXW(dev->index, "major = %d, minor = %d, pid = %d",
		imajor(inode), iminor(inode), current->pid);

	gps_each_link_close((enum gps_dl_link_id_enum)dev->index);
	gps_each_link_rec_force_dump(dev->index);

	return 0;
}

static int gps_each_device_hw_suspend(enum gps_dl_link_id_enum link_id, bool need_clk_ext)
{
	int pid;
	int retval;

	pid = current->pid;
	GDL_LOGXW(link_id, "pid = %d, clk_ext = %d", pid, need_clk_ext);

	retval = gps_each_link_hw_suspend(link_id, need_clk_ext);
	gps_each_link_rec_force_dump(link_id);

	return retval;
}

#define GPSDL_IOC_GPS_HWVER            6
#define GPSDL_IOC_GPS_IC_HW_VERSION    7
#define GPSDL_IOC_GPS_IC_FW_VERSION    8
#define GPSDL_IOC_D1_EFUSE_GET         9
#define GPSDL_IOC_RTC_FLAG             10
#define GPSDL_IOC_CO_CLOCK_FLAG        11
#define GPSDL_IOC_TRIGGER_ASSERT       12
#define GPSDL_IOC_QUERY_STATUS         13
#define GPSDL_IOC_TAKE_GPS_WAKELOCK    14
#define GPSDL_IOC_GIVE_GPS_WAKELOCK    15
#define GPSDL_IOC_GET_GPS_LNA_PIN      16
#define GPSDL_IOC_GPS_FWCTL            17
#define GPSDL_IOC_GPS_HW_SUSPEND       18
#define GPSDL_IOC_GPS_HW_RESUME        19
#define GPSDL_IOC_GPS_LISTEN_RST_EVT   20

static int gps_each_device_ioctl_inner(struct file *filp, unsigned int cmd, unsigned long arg, bool is_compat)
{
	struct gps_each_device *dev; /* device information */
	int retval;

#if 0
	dev = container_of(inode->i_cdev, struct gps_each_device, cdev);
	filp->private_data = dev; /* for other methods */
#endif
	dev = (struct gps_each_device *)(filp->private_data);

	GDL_LOGXD(dev->index, "cmd = %d, is_compat = %d", cmd, is_compat);
#if 0
	int retval = 0;
	ENUM_WMTHWVER_TYPE_T hw_ver_sym = WMTHWVER_INVALID;
	UINT32 hw_version = 0;
	UINT32 fw_version = 0;
	UINT32 gps_lna_pin = 0;
#endif
	switch (cmd) {
	case GPSDL_IOC_TRIGGER_ASSERT:
		/* Trigger FW assert for debug */
		GDL_LOGXW_DRW(dev->index, "GPSDL_IOC_TRIGGER_ASSERT, reason = %ld", arg);

		/* TODO: assert dev->is_open */
		if (dev->index == GPS_DATA_LINK_ID0)
			retval = gps_dl_trigger_gps_subsys_reset(false);
		else
			retval = gps_each_link_reset(dev->index);
		break;
	case GPSDL_IOC_QUERY_STATUS:
		retval = gps_each_link_check(dev->index, arg);
		gps_each_link_rec_force_dump(dev->index);
		GDL_LOGXW_DRW(dev->index, "GPSDL_IOC_QUERY_STATUS, reason = %ld, ret = %d", arg, retval);
		break;
	case GPSDL_IOC_CO_CLOCK_FLAG:
		retval = gps_dl_link_get_clock_flag();
		GDL_LOGXD_ONF(dev->index, "gps clock flag = 0x%x", retval);
		break;
#if 0
	case GPSDL_IOC_GPS_HWVER:
		/*get combo hw version */
		/* hw_ver_sym = mtk_wcn_wmt_hwver_get(); */

		GPS_DBG_FUNC("GPS_ioctl(): get hw version = %d, sizeof(hw_ver_sym) = %zd\n",
			      hw_ver_sym, sizeof(hw_ver_sym));
		if (copy_to_user((int __user *)arg, &hw_ver_sym, sizeof(hw_ver_sym)))
			retval = -EFAULT;

		break;
	case GPSDL_IOC_GPS_IC_HW_VERSION:
		/*get combo hw version from ic,  without wmt mapping */
		hw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);

		GPS_DBG_FUNC("GPS_ioctl(): get hw version = 0x%x\n", hw_version);
		if (copy_to_user((int __user *)arg, &hw_version, sizeof(hw_version)))
			retval = -EFAULT;

		break;

	case GPSDL_IOC_GPS_IC_FW_VERSION:
		/*get combo fw version from ic, without wmt mapping */
		fw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_FWVER);

		GPS_DBG_FUNC("GPS_ioctl(): get fw version = 0x%x\n", fw_version);
		if (copy_to_user((int __user *)arg, &fw_version, sizeof(fw_version)))
			retval = -EFAULT;

		break;
	case GPSDL_IOC_RTC_FLAG:

		retval = rtc_GPS_low_power_detected();

		GPS_DBG_FUNC("low power flag (%d)\n", retval);
		break;
	case GPSDL_IOC_CO_CLOCK_FLAG:
#if SOC_CO_CLOCK_FLAG
		retval = mtk_wcn_wmt_co_clock_flag_get();
#endif
		GPS_DBG_FUNC("GPS co_clock_flag (%d)\n", retval);
		break;
	case GPSDL_IOC_D1_EFUSE_GET:
#if defined(CONFIG_MACH_MT6735)
		do {
			char *addr = ioremap(0x10206198, 0x4);

			retval = *(unsigned int *)addr;
			GPS_DBG_FUNC("D1 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#elif defined(CONFIG_MACH_MT6763)
		do {
			char *addr = ioremap(0x11f10048, 0x4);

			retval = *(unsigned int *)addr;
			GPS_DBG_FUNC("MT6763 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#else
		GPS_ERR_FUNC("Read Efuse not supported in this platform\n");
#endif
		break;

	case GPSDL_IOC_TAKE_GPS_WAKELOCK:
		GPS_INFO_FUNC("Ioctl to take gps wakelock\n");
		gps_hold_wake_lock(1);
		if (wake_lock_acquired == 1)
			retval = 0;
		else
			retval = -EAGAIN;
		break;
	case GPSDL_IOC_GIVE_GPS_WAKELOCK:
		GPS_INFO_FUNC("Ioctl to give gps wakelock\n");
		gps_hold_wake_lock(0);
		if (wake_lock_acquired == 0)
			retval = 0;
		else
			retval = -EAGAIN;
		break;
#ifdef GPS_FWCTL_SUPPORT
	case GPSDL_IOC_GPS_FWCTL:
		GPS_INFO_FUNC("GPSDL_IOC_GPS_FWCTL\n");
		retval = GPS_fwctl((struct gps_fwctl_data *)arg);
		break;
#endif
	case GPSDL_IOC_GET_GPS_LNA_PIN:
		gps_lna_pin = mtk_wmt_get_gps_lna_pin_num();
		GPS_DBG_FUNC("GPS_ioctl(): get gps lna pin = %d\n", gps_lna_pin);
		if (copy_to_user((int __user *)arg, &gps_lna_pin, sizeof(gps_lna_pin)))
			retval = -EFAULT;
		break;
#endif
	case GPSDL_IOC_GPS_HW_SUSPEND:
		/* arg == 1 stand for need clk extension, otherwise is normal deep sotp mode */
		retval = gps_each_device_hw_suspend(dev->index, (arg == 1));
		GDL_LOGXI_ONF(dev->index,
			"GPSDL_IOC_GPS_HW_SUSPEND: arg = %ld, ret = %d", arg, retval);
		break;
	case GPSDL_IOC_GPS_HW_RESUME:
		retval = gps_each_device_hw_resume(dev->index);
		GDL_LOGXI_ONF(dev->index,
			"GPSDL_IOC_GPS_HW_RESUME: arg = %ld, ret = %d", arg, retval);
		break;
	case GPSDL_IOC_GPS_LISTEN_RST_EVT:
		retval = -EINVAL;
		GDL_LOGXI_ONF(dev->index,
			"GPSDL_IOC_GPS_LISTEN_RST_EVT retval = %d", retval);
		break;
	case 21505:
	case 21506:
	case 21515:
		/* known unsupported cmd */
		retval = -EFAULT;
		GDL_LOGXD_DRW(dev->index, "cmd = %d, not support", cmd);
		break;
	default:
		retval = -EFAULT;
		GDL_LOGXI_DRW(dev->index, "cmd = %d, not support", cmd);
		break;
	}

	return retval;
}


static long gps_each_device_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gps_each_device_ioctl_inner(filp, cmd, arg, false);
}

static long gps_each_device_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gps_each_device_ioctl_inner(filp, cmd, arg, true);
}

static const struct file_operations gps_each_device_fops = {
	.owner = THIS_MODULE,
	.open = gps_each_device_open,
	.read = gps_each_device_read,
	.write = gps_each_device_write,
	.release = gps_each_device_release,
	.unlocked_ioctl = gps_each_device_unlocked_ioctl,
	.compat_ioctl = gps_each_device_compat_ioctl,
};

int gps_dl_cdev_setup(struct gps_each_device *dev, int index)
{
	int result;

	init_waitqueue_head(&dev->r_wq);
	dev->i_len = 0;

	dev->index = index;
	/* assert dev->index == dev->cfg.index */

	cdev_init(&dev->cdev, &gps_each_device_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &gps_each_device_fops;

	result = cdev_add(&dev->cdev, dev->devno, 1);
	if (result) {
		GDL_LOGE("cdev_add error %d on index %d", result, index);
		return result;
	}

	dev->cls = class_create(THIS_MODULE, dev->cfg.dev_name);
	if (IS_ERR(dev->cls)) {
		GDL_LOGE("class_create fail on %s", dev->cfg.dev_name);
		return -1;
	}

	dev->dev = device_create(dev->cls, NULL, dev->devno, NULL, dev->cfg.dev_name);
	if (IS_ERR(dev->dev)) {
		GDL_LOGE("device_create fail on %s", dev->cfg.dev_name);
		return -1;
	}

	return 0;
}

void gps_dl_cdev_cleanup(struct gps_each_device *dev, int index)
{
	if (dev->dev) {
		device_destroy(dev->cls, dev->devno);
		dev->dev = NULL;
	}

	if (dev->cls) {
		class_destroy(dev->cls);
		dev->cls = NULL;
	}

	cdev_del(&dev->cdev);
}

