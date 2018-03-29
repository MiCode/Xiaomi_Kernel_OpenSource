/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
#include <linux/device.h>
#endif
#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"
#if defined(CONFIG_ARCH_MT6580)
#include <mt_clkbuf_ctl.h>
#endif
MODULE_LICENSE("GPL");

#define GPS_DRIVER_NAME "mtk_stp_GPS_chrdev"
#define GPS_DEV_MAJOR 191	/* never used number */
#define GPS_DEBUG_TRACE_GPIO         0
#define GPS_DEBUG_DUMP               0

#define PFX                         "[GPS] "
#define GPS_LOG_DBG                  3
#define GPS_LOG_INFO                 2
#define GPS_LOG_WARN                 1
#define GPS_LOG_ERR                  0

#define COMBO_IOC_GPS_HWVER           6
#define COMBO_IOC_GPS_IC_HW_VERSION   7
#define COMBO_IOC_GPS_IC_FW_VERSION   8
#define COMBO_IOC_D1_EFUSE_GET       9
#define COMBO_IOC_RTC_FLAG	     10
#define COMBO_IOC_CO_CLOCK_FLAG	     11
#define COMBO_IOC_TRIGGER_WMT_ASSERT 12

static UINT32 gDbgLevel = GPS_LOG_DBG;

#define GPS_DBG_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_debug(PFX "[D]%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_INFO_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_INFO)	\
		pr_info(PFX "[I]%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_WARN_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_WARN)	\
		pr_warn(PFX "[W]%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_ERR_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_ERR)	\
		pr_err(PFX "[E]%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_TRC_FUNC(f)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_info(PFX "<%s> <%d>\n", __func__, __LINE__);	\
} while (0)

static int GPS_devs = 1;	/* device count */
static int GPS_major = GPS_DEV_MAJOR;	/* dynamic allocation */
module_param(GPS_major, uint, 0);
static struct cdev GPS_cdev;

static struct wakeup_source gps_wake_lock;
static unsigned char wake_lock_acquired;   /* default: 0 */

#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MT_ENG_BUILD))
#define STP_GPS_BUFFER_SIZE 2048
#else
#define STP_GPS_BUFFER_SIZE MTKSTP_BUFFER_SIZE
#endif
static unsigned char i_buf[STP_GPS_BUFFER_SIZE];	/* input buffer of read() */
static unsigned char o_buf[STP_GPS_BUFFER_SIZE];	/* output buffer of write() */
static struct semaphore wr_mtx, rd_mtx;
static DECLARE_WAIT_QUEUE_HEAD(GPS_wq);
static int flag;
static volatile int retflag;

static void GPS_event_cb(void);

static void gps_hold_wake_lock(int hold)
{
	if (hold == 1) {
		if (!wake_lock_acquired) {
			GPS_DBG_FUNC("acquire gps wake_lock acquired = %d\n", wake_lock_acquired);
			__pm_stay_awake(&gps_wake_lock);
			wake_lock_acquired = 1;
		}
	} else if (hold == 0) {
		if (wake_lock_acquired) {
			GPS_DBG_FUNC("release gps wake_lock acquired = %d\n", wake_lock_acquired);
			__pm_relax(&gps_wake_lock);
			wake_lock_acquired = 0;
		}
	}
}

bool rtc_GPS_low_power_detected(void)
{
	static bool first_query = true;

	if (first_query) {
		first_query = false;
		/*return rtc_low_power_detected();*/
		return 0;
	} else {
		return false;
	}
}
ssize_t GPS_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	int written = 0;

	down(&wr_mtx);

	/* GPS_TRC_FUNC(); */

	/*pr_warn("%s: count %d pos %lld\n", __func__, count, *f_pos); */
	if (count > 0) {
		int copy_size = (count < MTKSTP_BUFFER_SIZE) ? count : MTKSTP_BUFFER_SIZE;

		if (copy_from_user(&o_buf[0], &buf[0], copy_size)) {
			retval = -EFAULT;
			goto out;
		}
		/* pr_warn("%02x ", val); */
#if GPS_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_TX, DBG_TIE_LOW);
#endif
		written = mtk_wcn_stp_send_data(&o_buf[0], copy_size, GPS_TASK_INDX);
#if GPS_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_TX, DBG_TIE_HIGH);
#endif

#if GPS_DEBUG_DUMP
		{
			unsigned char *buf_ptr = &o_buf[0];
			int k = 0;

			pr_warn("--[GPS-WRITE]--");
			for (k = 0; k < 10; k++) {
				if (k % 16 == 0)
					pr_warn("\n");
				pr_warn("0x%02x ", o_buf[k]);
			}
			pr_warn("\n");
		}
#endif
		/*
		   If cannot send successfully, enqueue again

		   if (written != copy_size) {
		   // George: FIXME! Move GPS retry handling from app to driver
		   }
		 */
		if (0 == written) {
			retval = -ENOSPC;
			/*no windowspace in STP is available,
			native process should not call GPS_write with no delay at all */
			GPS_ERR_FUNC
			    ("target packet length:%zd, write success length:%d, retval = %d.\n",
			     count, written, retval);
		} else {
			retval = written;
		}
	} else {
		retval = -EFAULT;
		GPS_ERR_FUNC("target packet length:%zd is not allowed, retval = %d.\n", count, retval);
	}
out:
	up(&wr_mtx);
	return retval;
}

ssize_t GPS_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	long val = 0;
	int retval;

	down(&rd_mtx);

/*    pr_warn("GPS_read(): count %d pos %lld\n", count, *f_pos);*/

	if (count > MTKSTP_BUFFER_SIZE)
		count = MTKSTP_BUFFER_SIZE;

#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif
	retval = mtk_wcn_stp_receive_data(i_buf, count, GPS_TASK_INDX);
#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif

	while (retval == 0) {
		/* got nothing, wait for STP's signal */
		/*wait_event(GPS_wq, flag != 0); *//* George: let signal wake up */
		val = wait_event_interruptible(GPS_wq, flag != 0);
		flag = 0;

#if GPS_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif

		retval = mtk_wcn_stp_receive_data(i_buf, count, GPS_TASK_INDX);

#if GPS_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif
		/* if we are signaled */
		if (val) {
			if (-ERESTARTSYS == val)
				GPS_DBG_FUNC("signaled by -ERESTARTSYS(%ld)\n ", val);
			else
				GPS_DBG_FUNC("signaled by %ld\n ", val);

			break;
		}
	}

#if GPS_DEBUG_DUMP
	{
		unsigned char *buf_ptr = &i_buf[0];
		int k = 0;

		pr_warn("--[GPS-READ]--");
		for (k = 0; k < 10; k++) {
			if (k % 16 == 0)
				pr_warn("\n");
			pr_warn("0x%02x ", i_buf[k]);
		}
		pr_warn("--\n");
	}
#endif

	if (retval > 0) {
		/* we got something from STP driver */
		if (copy_to_user(buf, i_buf, retval)) {
			retval = -EFAULT;
			goto OUT;
		} else {
			/* success */
		}
	} else {
		/* we got nothing from STP driver, being signaled */
		retval = val;
	}

OUT:
	up(&rd_mtx);
/*    pr_warn("GPS_read(): retval = %d\n", retval);*/
	return retval;
}

/* int GPS_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) */
long GPS_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	ENUM_WMTHWVER_TYPE_T hw_ver_sym = WMTHWVER_INVALID;
	UINT32 hw_version = 0;
	UINT32 fw_version = 0;

	pr_warn("GPS_ioctl(): cmd (%d)\n", cmd);

	switch (cmd) {
	case 0:		/* enable/disable STP */
		GPS_DBG_FUNC("GPS_ioctl(): disable STP control from GPS dev\n");
		retval = -EINVAL;
#if 1
#else
		/* George: STP is controlled by WMT only */
		mtk_wcn_stp_enable(arg);
#endif
		break;

	case 1:		/* send raw data */
		GPS_DBG_FUNC("GPS_ioctl(): disable raw data from GPS dev\n");
		retval = -EINVAL;
		break;

	case COMBO_IOC_GPS_HWVER:
		/*get combo hw version */
		hw_ver_sym = mtk_wcn_wmt_hwver_get();

		GPS_DBG_FUNC("GPS_ioctl(): get hw version = %d, sizeof(hw_ver_sym) = %zd\n",
			      hw_ver_sym, sizeof(hw_ver_sym));
		if (copy_to_user((int __user *)arg, &hw_ver_sym, sizeof(hw_ver_sym)))
			retval = -EFAULT;

		break;
	case COMBO_IOC_GPS_IC_HW_VERSION:
		/*get combo hw version from ic,  without wmt mapping */
		hw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);

		GPS_DBG_FUNC("GPS_ioctl(): get hw version = 0x%x\n", hw_version);
		if (copy_to_user((int __user *)arg, &hw_version, sizeof(hw_version)))
			retval = -EFAULT;

		break;

	case COMBO_IOC_GPS_IC_FW_VERSION:
		/*get combo fw version from ic, without wmt mapping */
		fw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_FWVER);

		GPS_DBG_FUNC("GPS_ioctl(): get fw version = 0x%x\n", fw_version);
		if (copy_to_user((int __user *)arg, &fw_version, sizeof(fw_version)))
			retval = -EFAULT;

		break;
	case COMBO_IOC_RTC_FLAG:

		retval = rtc_GPS_low_power_detected();

		GPS_DBG_FUNC("low power flag (%d)\n", retval);
		break;
	case COMBO_IOC_CO_CLOCK_FLAG:
#if SOC_CO_CLOCK_FLAG
		retval = mtk_wcn_wmt_co_clock_flag_get();
#endif
		GPS_DBG_FUNC("GPS co_clock_flag (%d)\n", retval);
		break;
	case COMBO_IOC_D1_EFUSE_GET:
#if defined(CONFIG_ARCH_MT6735)
		do {
			char *addr = ioremap(0x10206198, 0x4);

			retval = *(volatile unsigned int *)addr;
			GPS_DBG_FUNC("D1 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#else
		GPS_ERR_FUNC("Read Efuse not supported in this platform\n");
#endif
		break;

	case COMBO_IOC_TRIGGER_WMT_ASSERT:
		/* Trigger FW assert for debug */
		GPS_INFO_FUNC("%s: Host trigger FW assert......, reason:%lu\n", __func__, arg);
		retval = mtk_wcn_wmt_assert(WMTDRV_TYPE_GPS, arg);
		if (retval == MTK_WCN_BOOL_TRUE) {
			GPS_INFO_FUNC("Host trigger FW assert succeed\n");
			retval = 0;
		} else {
			GPS_ERR_FUNC("Host trigger FW assert Failed\n");
			retval = (-EBUSY);
		}
		break;

	default:
		retval = -EFAULT;
		GPS_DBG_FUNC("GPS_ioctl(): unknown cmd (%d)\n", cmd);
		break;
	}

/*OUT:*/
	return retval;
}

long GPS_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	pr_warn("%s: cmd (%d)\n", __func__, cmd);
	ret = GPS_unlocked_ioctl(filp, cmd, arg);
	pr_warn("%s: cmd (%d)\n", __func__, cmd);
	return ret;
}

static void gps_cdev_rst_cb(ENUM_WMTDRV_TYPE_T src,
			    ENUM_WMTDRV_TYPE_T dst, ENUM_WMTMSG_TYPE_T type, void *buf, unsigned int sz)
{

	/*
	   To handle reset procedure please
	 */
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	GPS_DBG_FUNC("sizeof(ENUM_WMTRSTMSG_TYPE_T) = %zd\n", sizeof(ENUM_WMTRSTMSG_TYPE_T));
	if (sz <= sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		memcpy((char *)&rst_msg, (char *)buf, sz);
		GPS_DBG_FUNC("src = %d, dst = %d, type = %d, buf = 0x%x sz = %d, max = %d\n", src,
			      dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);
		if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_GPS)
		    && (type == WMTMSG_TYPE_RESET)) {
			if (rst_msg == WMTRSTMSG_RESET_START) {
				GPS_DBG_FUNC("gps restart start!\n");

				/*reset_start message handling */
				retflag = 1;

			} else if ((rst_msg == WMTRSTMSG_RESET_END) || (rst_msg == WMTRSTMSG_RESET_END_FAIL)) {
				GPS_DBG_FUNC("gps restart end!\n");

				/*reset_end message handling */
				retflag = 0;
			}
		}
	} else {
		/*message format invalid */
	}
}

static int GPS_open(struct inode *inode, struct file *file)
{
	pr_debug("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;
	if (retflag == 1) {
		GPS_WARN_FUNC("whole chip resetting...\n");
		return -EPERM;
	}

#if 1				/* GeorgeKuo: turn on function before check stp ready */
	/* turn on BT */

	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS)) {
		GPS_WARN_FUNC("WMT turn on GPS fail!\n");
		return -ENODEV;
	}

	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_GPS, gps_cdev_rst_cb);
	GPS_DBG_FUNC("WMT turn on GPS OK!\n");

#endif

	if (mtk_wcn_stp_is_ready()) {
#if 0
		if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS)) {
			GPS_WARN_FUNC("WMT turn on GPS fail!\n");
			return -ENODEV;
		}
		GPS_DBG_FUNC("WMT turn on GPS OK!\n");
#endif
		mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, GPS_event_cb);
	} else {
		GPS_ERR_FUNC("STP is not ready, Cannot open GPS Devices\n\r");

		/*return error code */
		return -ENODEV;
	}
	gps_hold_wake_lock(1);
	GPS_DBG_FUNC("gps_hold_wake_lock(1)\n");
#if defined(CONFIG_ARCH_MT6580)
	clk_buf_ctrl(CLK_BUF_AUDIO, 1);
#endif
	/* init_MUTEX(&wr_mtx); */
	sema_init(&wr_mtx, 1);
	/* init_MUTEX(&rd_mtx); */
	sema_init(&rd_mtx, 1);

	return 0;
}

static int GPS_close(struct inode *inode, struct file *file)
{
	pr_debug("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;
	if (retflag == 1) {
		GPS_WARN_FUNC("whole chip resetting...\n");
		return -EPERM;
	}

	/*Flush Rx Queue */
	mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, 0x0);	/* unregister event callback function */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPS);

	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_GPS)) {
		GPS_WARN_FUNC("WMT turn off GPS fail!\n");
		return -EIO;	/* mostly, native programer does not care this return vlaue,
		but we still return error code. */
	}
	GPS_DBG_FUNC("WMT turn off GPS OK!\n");

	gps_hold_wake_lock(0);
	GPS_DBG_FUNC("gps_hold_wake_lock(0)\n");

#if defined(CONFIG_ARCH_MT6580)
	clk_buf_ctrl(CLK_BUF_AUDIO, 0);
#endif
	return 0;
}

const struct file_operations GPS_fops = {
	.open = GPS_open,
	.release = GPS_close,
	.read = GPS_read,
	.write = GPS_write,
/* .ioctl = GPS_ioctl */
	.unlocked_ioctl = GPS_unlocked_ioctl,
	.compat_ioctl = GPS_compat_ioctl,
};

void GPS_event_cb(void)
{
/*    pr_debug("GPS_event_cb()\n");*/

	flag = 1;
	wake_up(&GPS_wq);
}

#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
struct class *stpgps_class = NULL;
#endif

static int GPS_init(void)
{
	dev_t dev = MKDEV(GPS_major, 0);
	int alloc_ret = 0;
	int cdev_err = 0;
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	struct device *stpgps_dev = NULL;
#endif

	/*static allocate chrdev */
	alloc_ret = register_chrdev_region(dev, 1, GPS_DRIVER_NAME);
	if (alloc_ret) {
		pr_warn("fail to register chrdev\n");
		return alloc_ret;
	}

	cdev_init(&GPS_cdev, &GPS_fops);
	GPS_cdev.owner = THIS_MODULE;

	cdev_err = cdev_add(&GPS_cdev, dev, GPS_devs);
	if (cdev_err)
		goto error;
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE

	stpgps_class = class_create(THIS_MODULE, "stpgps");
	if (IS_ERR(stpgps_class))
		goto error;
	stpgps_dev = device_create(stpgps_class, NULL, dev, NULL, "stpgps");
	if (IS_ERR(stpgps_dev))
		goto error;
#endif
	pr_warn("%s driver(major %d) installed.\n", GPS_DRIVER_NAME, GPS_major);

	wakeup_source_init(&gps_wake_lock, "gpswakelock");

	return 0;

error:

#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	if (!IS_ERR(stpgps_dev))
		device_destroy(stpgps_class, dev);
	if (!IS_ERR(stpgps_class)) {
		class_destroy(stpgps_class);
		stpgps_class = NULL;
	}
#endif
	if (cdev_err == 0)
		cdev_del(&GPS_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, GPS_devs);

	return -1;
}

static void GPS_exit(void)
{
	dev_t dev = MKDEV(GPS_major, 0);
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	device_destroy(stpgps_class, dev);
	class_destroy(stpgps_class);
	stpgps_class = NULL;
#endif

	cdev_del(&GPS_cdev);
	unregister_chrdev_region(dev, GPS_devs);
	pr_warn("%s driver removed.\n", GPS_DRIVER_NAME);

	wakeup_source_trash(&gps_wake_lock);
}

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE

int mtk_wcn_stpgps_drv_init(void)
{
	return GPS_init();
}
EXPORT_SYMBOL(mtk_wcn_stpgps_drv_init);

void mtk_wcn_stpgps_drv_exit(void)
{
	return GPS_exit();
}
EXPORT_SYMBOL(mtk_wcn_stpgps_drv_exit);
#else

module_init(GPS_init);
module_exit(GPS_exit);

#endif
