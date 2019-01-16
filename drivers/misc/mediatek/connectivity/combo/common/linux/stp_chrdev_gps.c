/** $Log: stp_chrdev_gps.c $
 *
 * 12 13 2010 Sean.Wang
 * (1) Add GPS_DEBUG_TRACE_GPIO to disable GPIO debugging trace
 * (2) Add GPS_DEBUG_DUMP to support GPS data dump
 * (3) Add mtk_wcn_stp_is_ready() check in GPS_open()
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/device.h>

#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"

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

static UINT32 gDbgLevel = GPS_LOG_DBG;

#define GPS_DBG_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_warn(PFX "%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_INFO_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_INFO)	\
		pr_warn(PFX "%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_WARN_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_WARN)	\
		pr_err(PFX "%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_ERR_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_ERR)	\
		pr_err(PFX "%s: "  fmt, __func__ , ##arg);	\
} while (0)
#define GPS_TRC_FUNC(f)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_info(PFX "<%s> <%d>\n", __func__, __LINE__);	\
} while (0)


static INT32 GPS_devs = 1;	/* device count */
static INT32 GPS_major = GPS_DEV_MAJOR;	/* dynamic allocation */
module_param(GPS_major, uint, 0);
static struct cdev GPS_cdev;

static UINT8 i_buf[MTKSTP_BUFFER_SIZE];	/* input buffer of read() */
static UINT8 o_buf[MTKSTP_BUFFER_SIZE];	/* output buffer of write() */
static struct semaphore wr_mtx, rd_mtx;
static DECLARE_WAIT_QUEUE_HEAD(GPS_wq);
static INT32 flag;

static VOID GPS_event_cb(VOID);

ssize_t GPS_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 retval = 0;
	INT32 written = 0;
	down(&wr_mtx);

	/* GPS_TRC_FUNC(); */

	/*pr_warn("%s: count %d pos %lld\n", __func__, count, *f_pos); */
	if (count > 0) {
		INT32 copy_size = (count < MTKSTP_BUFFER_SIZE) ? count : MTKSTP_BUFFER_SIZE;
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
			PUINT8 buf_ptr = &o_buf[0];
			INT32 k = 0;
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
			/*no windowspace in STP is available, native process should not call GPS_write with no delay at all */
			GPS_ERR_FUNC
			    ("target packet length:%zd, write success length:%d, retval = %d.\n",
			     count, written, retval);
		} else {
			retval = written;
		}
	} else {
		retval = -EFAULT;
		GPS_ERR_FUNC("target packet length:%zd is not allowed, retval = %d.\n", count,
			     retval);
	}
 out:
	up(&wr_mtx);
	return (retval);
}

ssize_t GPS_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 val = 0;
	INT32 retval;

	down(&rd_mtx);

/*    pr_warn("GPS_read(): count %d pos %lld\n", count, *f_pos);*/

	if (count > MTKSTP_BUFFER_SIZE) {
		count = MTKSTP_BUFFER_SIZE;
	}
#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif
	retval = mtk_wcn_stp_receive_data(i_buf, count, GPS_TASK_INDX);
#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif

	while (retval == 0)	/* got nothing, wait for STP's signal */
	{
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
			if (-ERESTARTSYS == val) {
				GPS_INFO_FUNC("signaled by -ERESTARTSYS(%d)\n ", val);
			} else {
				GPS_INFO_FUNC("signaled by %d\n ", val);
			}
			break;
		}
	}

#if GPS_DEBUG_DUMP
	{
		PUINT8 buf_ptr = &i_buf[0];
		INT32 k = 0;
		pr_warn("--[GPS-READ]--");
		for (k = 0; k < 10; k++) {
			if (k % 16 == 0)
				pr_warn("\n");
			pr_warn("0x%02x ", i_buf[k]);
		}
		pr_warn("--\n");
	}
#endif

	if (retval) {
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
	return (retval);
}

/* int GPS_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) */
long GPS_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	INT32 retval = 0;
	ENUM_WMTHWVER_TYPE_T hw_ver_sym = WMTHWVER_INVALID;
	UINT32 hw_version = 0;
	UINT32 fw_version = 0;
	pr_warn("GPS_ioctl(): cmd (%d)\n", cmd);

	switch (cmd) {
	case 0:		/* enable/disable STP */
		GPS_INFO_FUNC(KERN_INFO "GPS_ioctl(): disable STP control from GPS dev\n");
		retval = -EINVAL;
#if 1
#else
		/* George: STP is controlled by WMT only */
		mtk_wcn_stp_enable(arg);
#endif
		break;

	case 1:		/* send raw data */
		GPS_INFO_FUNC(KERN_INFO "GPS_ioctl(): disable raw data from GPS dev\n");
		retval = -EINVAL;
		break;

	case COMBO_IOC_GPS_HWVER:
		/*get combo hw version */
		hw_ver_sym = mtk_wcn_wmt_hwver_get();

		GPS_INFO_FUNC(KERN_INFO
			      "GPS_ioctl(): get hw version = %d, sizeof(hw_ver_sym) = %zd\n",
			      hw_ver_sym, sizeof(hw_ver_sym));
		if (copy_to_user((int __user *)arg, &hw_ver_sym, sizeof(hw_ver_sym))) {
			retval = -EFAULT;
		}
		break;
	case COMBO_IOC_GPS_IC_HW_VERSION:
		/*get combo hw version from ic,  without wmt mapping */
		hw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);

		GPS_INFO_FUNC(KERN_INFO "GPS_ioctl(): get hw version = 0x%x\n", hw_version);
		if (copy_to_user((int __user *)arg, &hw_version, sizeof(hw_version))) {
			retval = -EFAULT;
		}
		break;

	case COMBO_IOC_GPS_IC_FW_VERSION:
		/*get combo fw version from ic, without wmt mapping */
		fw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_FWVER);

		GPS_INFO_FUNC(KERN_INFO "GPS_ioctl(): get fw version = 0x%x\n", fw_version);
		if (copy_to_user((int __user *)arg, &fw_version, sizeof(fw_version))) {
			retval = -EFAULT;
		}
		break;
	default:
		retval = -EFAULT;
		GPS_INFO_FUNC(KERN_INFO "GPS_ioctl(): unknown cmd (%d)\n", cmd);
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

static VOID gps_cdev_rst_cb(ENUM_WMTDRV_TYPE_T src,
			    ENUM_WMTDRV_TYPE_T dst,
			    ENUM_WMTMSG_TYPE_T type, PVOID buf, UINT32 sz)
{

	/*
	   To handle reset procedure please
	 */
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	GPS_INFO_FUNC("sizeof(ENUM_WMTRSTMSG_TYPE_T) = %zd\n", sizeof(ENUM_WMTRSTMSG_TYPE_T));
	if (sz <= sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		memcpy((PINT8)&rst_msg, (PINT8)buf, sz);
		GPS_INFO_FUNC("src = %d, dst = %d, type = %d, buf = 0x%x sz = %d, max = %d\n", src,
			      dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);
		if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_GPS)
		    && (type == WMTMSG_TYPE_RESET)) {
			if (rst_msg == WMTRSTMSG_RESET_START) {
				GPS_INFO_FUNC("gps restart start!\n");

				/*reset_start message handling */

                    } else if((rst_msg == WMTRSTMSG_RESET_END) || (rst_msg == WMTRSTMSG_RESET_END_FAIL)){
				GPS_INFO_FUNC("gps restart end!\n");

				/*reset_end message handling */
			}
		}
	} else {
		/*message format invalid */
	}
}

static int GPS_open(struct inode *inode, struct file *file)
{
	pr_warn("%s: major %d minor %d (pid %d)\n", __func__,
		imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;

#if 1				/* GeorgeKuo: turn on function before check stp ready */
	/* turn on BT */

	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS)) {
		GPS_WARN_FUNC("WMT turn on GPS fail!\n");
		return -ENODEV;
	} else {
		mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_GPS, gps_cdev_rst_cb);
		GPS_INFO_FUNC("WMT turn on GPS OK!\n");
	}
#endif

	if (mtk_wcn_stp_is_ready()) {
#if 0
		if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS)) {
			GPS_WARN_FUNC("WMT turn on GPS fail!\n");
			return -ENODEV;
		}
		GPS_INFO_FUNC("WMT turn on GPS OK!\n");
#endif
		mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, GPS_event_cb);
	} else {
		GPS_ERR_FUNC("STP is not ready, Cannot open GPS Devices\n\r");

		/*return error code */
		return -ENODEV;
	}

	/* init_MUTEX(&wr_mtx); */
	sema_init(&wr_mtx, 1);
	/* init_MUTEX(&rd_mtx); */
	sema_init(&rd_mtx, 1);

	return 0;
}

static int GPS_close(struct inode *inode, struct file *file)
{
	pr_warn("%s: major %d minor %d (pid %d)\n", __func__,
		imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;

	/*Flush Rx Queue */
	mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, 0x0);	/* unregister event callback function */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPS);

	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_GPS)) {
		GPS_WARN_FUNC("WMT turn off GPS fail!\n");
		return -EIO;	/* mostly, native programer does not care this return vlaue, but we still return error code. */
	} else {
		GPS_INFO_FUNC("WMT turn off GPS OK!\n");
	}

	return 0;
}

struct file_operations GPS_fops = {
	.open = GPS_open,
	.release = GPS_close,
	.read = GPS_read,
	.write = GPS_write,
/* .ioctl = GPS_ioctl */
	.unlocked_ioctl = GPS_unlocked_ioctl,
	.compat_ioctl = GPS_compat_ioctl,
};

VOID GPS_event_cb(VOID)
{
/*    pr_warn("GPS_event_cb()\n");*/

	flag = 1;
	wake_up(&GPS_wq);

	return;
}

#if REMOVE_MK_NODE
struct class *stpgps_class = NULL;
#endif

static int GPS_init(void)
{
	dev_t dev = MKDEV(GPS_major, 0);
	INT32 alloc_ret = 0;
	INT32 cdev_err = 0;
#if REMOVE_MK_NODE
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
#if REMOVE_MK_NODE

	stpgps_class = class_create(THIS_MODULE, "stpgps");
	if (IS_ERR(stpgps_class))
		goto error;
	stpgps_dev = device_create(stpgps_class, NULL, dev, NULL, "stpgps");
	if (IS_ERR(stpgps_dev))
		goto error;
#endif
	pr_warn(KERN_ALERT "%s driver(major %d) installed.\n", GPS_DRIVER_NAME, GPS_major);

	return 0;

 error:

#if REMOVE_MK_NODE
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
#if REMOVE_MK_NODE
	device_destroy(stpgps_class, dev);
	class_destroy(stpgps_class);
	stpgps_class = NULL;
#endif

	cdev_del(&GPS_cdev);
	unregister_chrdev_region(dev, GPS_devs);

	pr_warn(KERN_ALERT "%s driver removed.\n", GPS_DRIVER_NAME);
}


#ifdef MTK_WCN_REMOVE_KERNEL_MODULE

INT32 mtk_wcn_stpgps_drv_init(VOID)
{
	return GPS_init();
}

VOID mtk_wcn_stpgps_drv_exit(VOID)
{
	return GPS_exit();
}


EXPORT_SYMBOL(mtk_wcn_stpgps_drv_init);
EXPORT_SYMBOL(mtk_wcn_stpgps_drv_exit);
#else

module_init(GPS_init);
module_exit(GPS_exit);

#endif
