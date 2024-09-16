/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <asm/div64.h>
#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"
#include <connectivity_build_in_adapter.h>
#if defined(CONFIG_MACH_MT6580)
#include <mt_clkbuf_ctl.h>
#endif
#include "gps.h"
#if defined(CONFIG_MACH_MT6739)
#include <mtk_freqhopping.h>
#include <mtk_freqhopping_drv.h>
#endif
#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761)
#include <helio-dvfsrc.h>
#endif
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
#include "gps_lna_drv.h"
#endif

#define GPS2_DEBUG_TRACE_GPIO         0
#define GPS2_DEBUG_DUMP               0

#define PFX2                         "[GPS2] "
#define GPS2_LOG_DBG                  3
#define GPS2_LOG_INFO                 2
#define GPS2_LOG_WARN                 1
#define GPS2_LOG_ERR                  0

#define COMBO_IOC_GPS2_HWVER           6
#define COMBO_IOC_GPS2_IC_HW_VERSION   7
#define COMBO_IOC_GPS2_IC_FW_VERSION   8
#define COMBO_IOC_D1_EFUSE_GET2        9
#define COMBO_IOC_RTC_FLAG2           10
#define COMBO_IOC_CO_CLOCK_FLAG2      11
#define COMBO_IOC_TRIGGER_WMT_ASSERT2 12
#define COMBO_IOC_WMT_STATUS2         13
#define COMBO_IOC_TAKE_GPS2_WAKELOCK  14
#define COMBO_IOC_GIVE_GPS2_WAKELOCK  15
#define COMBO_IOC_GET_GPS2_LNA_PIN    16
#define COMBO_IOC_GPS2_FWCTL          17
#define COMBO_IOC_GPS2_HW_SUSPEND     18
#define COMBO_IOC_GPS2_HW_RESUME      19
#define COMBO_IOC_GPS2_LISTEN_RST_EVT 20

static UINT32 g2DbgLevel = GPS2_LOG_DBG;

#define GPS2_DBG_FUNC(fmt, arg...)	\
do { if (g2DbgLevel >= GPS2_LOG_DBG)	\
		pr_debug(PFX2 "[D]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS2_INFO_FUNC(fmt, arg...)	\
do { if (g2DbgLevel >= GPS2_LOG_INFO)	\
		pr_info(PFX2 "[I]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS2_WARN_FUNC(fmt, arg...)	\
do { if (g2DbgLevel >= GPS2_LOG_WARN)	\
		pr_info(PFX2 "[W]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS2_ERR_FUNC(fmt, arg...)	\
do { if (g2DbgLevel >= GPS2_LOG_ERR)	\
		pr_info(PFX2 "[E]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS2_TRC_FUNC(f)	\
do { if (g2DbgLevel >= GPS2_LOG_DBG)	\
		pr_info(PFX2 "<%s> <%d>\n", __func__, __LINE__);	\
} while (0)

struct wakeup_source gps2_wake_lock;
static unsigned char wake_lock_acquired2;   /* default: 0 */

#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_ENG_BUILD))
#define STP_GPS_BUFFER_SIZE2 2048
#else
#define STP_GPS_BUFFER_SIZE2 MTKSTP_BUFFER_SIZE
#endif
static unsigned char i_buf2[STP_GPS_BUFFER_SIZE2];	/* input buffer of read() */
static unsigned char o_buf2[STP_GPS_BUFFER_SIZE2];	/* output buffer of write() */
struct semaphore wr_mtx2, rd_mtx2, status_mtx2;
static DECLARE_WAIT_QUEUE_HEAD(GPS2_wq);
static DECLARE_WAIT_QUEUE_HEAD(GPS2_rst_wq);
static int flag2;
static int rstflag2;
static int rst_listening_flag2;
static int rst_happened_or_gps_close_flag2;

static enum gps_ctrl_status_enum g_gps2_ctrl_status;

static void GPS2_check_and_wakeup_rst_listener(enum gps_ctrl_status_enum to)
{
	if (to == GPS_RESET_START || to == GPS_RESET_DONE  || to == GPS_CLOSED) {
		rst_happened_or_gps_close_flag2 = 1;
		if (rst_listening_flag2) {
			wake_up(&GPS2_rst_wq);
			GPS2_WARN_FUNC("Set GPS rst_happened_or_gps_close_flag2 because to = %d", to);
		}
	}
}

#ifdef GPS_HW_SUSPEND_SUPPORT
static void GPS2_ctrl_status_change_from_to(enum gps_ctrl_status_enum from, enum gps_ctrl_status_enum to)
{
	bool okay = true;
	enum gps_ctrl_status_enum status_backup;

	down(&status_mtx2);
	/* Due to checking status and setting status are not atomic,
	 * mutex is needed to make sure they are an atomic operation.
	 * Note: reading the status is no need to protect
	 */
	status_backup = g_gps2_ctrl_status;
	if (status_backup == from)
		g_gps2_ctrl_status = to;
	else
		okay = false;
	up(&status_mtx2);

	if (!okay)
		GPS2_WARN_FUNC("GPS unexpected status %d, not chagne from %d to %d", status_backup, from, to);
	else
		GPS2_check_and_wakeup_rst_listener(to);
}
#endif

static enum gps_ctrl_status_enum GPS2_ctrl_status_change_to(enum gps_ctrl_status_enum to)
{
	enum gps_ctrl_status_enum status_backup;

	down(&status_mtx2);
	status_backup = g_gps2_ctrl_status;
	g_gps2_ctrl_status = to;
	up(&status_mtx2);

	GPS2_check_and_wakeup_rst_listener(to);

	return status_backup;
}

static void GPS2_event_cb(void);

static void gps2_hold_wake_lock(int hold)
{
	if (hold == 1) {
		if (!wake_lock_acquired2) {
			GPS2_DBG_FUNC("acquire gps2 wake_lock acquired = %d\n", wake_lock_acquired2);
			__pm_stay_awake(&gps2_wake_lock);
			wake_lock_acquired2 = 1;
		} else {
			GPS2_DBG_FUNC("acquire gps2 wake_lock acquired = %d (do nothing)\n", wake_lock_acquired2);
		}
	} else if (hold == 0) {
		if (wake_lock_acquired2) {
			GPS2_DBG_FUNC("release gps2 wake_lock acquired = %d\n", wake_lock_acquired2);
			__pm_relax(&gps2_wake_lock);
			wake_lock_acquired2 = 0;
		} else {
			GPS2_DBG_FUNC("release gps2 wake_lock acquired = %d (do nothing)\n", wake_lock_acquired2);
		}
	}
}

bool rtc_GPS2_low_power_detected(void)
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

ssize_t GPS2_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	int written = 0;

	down(&wr_mtx2);

	/* GPS2_TRC_FUNC(); */

	/*pr_info("%s: count %d pos %lld\n", __func__, count, *f_pos); */
	if (count > 0) {
		int copy_size = (count < STP_GPS_BUFFER_SIZE2) ? count : STP_GPS_BUFFER_SIZE2;

		if (copy_from_user(&o_buf2[0], &buf[0], copy_size)) {
			retval = -EFAULT;
			goto out;
		}
		/* pr_info("%02x ", val); */
#if GPS2_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_TX, DBG_TIE_LOW);
#endif
		written = mtk_wcn_stp_send_data(&o_buf2[0], copy_size, GPSL5_TASK_INDX);
#if GPS2_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_TX, DBG_TIE_HIGH);
#endif

#if GPS2_DEBUG_DUMP
		{
			unsigned char *buf_ptr = &o_buf2[0];
			int k = 0;

			pr_info("--[GPS2-WRITE]--");
			for (k = 0; k < 10; k++) {
				if (k % 16 == 0)
					pr_info("\n");
				pr_info("0x%02x ", o_buf2[k]);
			}
			pr_info("\n");
		}
#endif
		if (written == 0) {
			retval = -ENOSPC;
			/* no windowspace in STP is available, */
			/* native process should not call GPS2_write with no delay at all */
			GPS2_ERR_FUNC
			    ("target packet length:%zd, write success length:%d, retval = %d.\n",
			     count, written, retval);
		} else {
			retval = written;
		}
	} else {
		retval = -EFAULT;
		GPS2_ERR_FUNC("target packet length:%zd is not allowed, retval = %d.\n", count, retval);
	}
out:
	up(&wr_mtx2);
	return retval;
}

ssize_t GPS2_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	long val = 0;
	int retval;

	down(&rd_mtx2);

    /* pr_debug("GPS2_read(): count %d pos %lld\n", count, *f_pos); */
	if (rstflag2 == 1) {
		if (filp->f_flags & O_NONBLOCK) {
			/* GPS2_DBG_FUNC("Non-blocking read, whole chip reset occurs! rstflag2=%d\n", rstflag2); */
			retval = -EIO;
			goto OUT;
		}
	}

	if (count > STP_GPS_BUFFER_SIZE2)
		count = STP_GPS_BUFFER_SIZE2;

#if GPS2_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif
	retval = mtk_wcn_stp_receive_data(i_buf2, count, GPSL5_TASK_INDX);
#if GPS2_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif

	while (retval == 0) {
		/* got nothing, wait for STP's signal */
		/* wait_event(GPS2_wq, flag2 != 0); *//* George: let signal wake up */
		if (filp->f_flags & O_NONBLOCK) {
			/* GPS2_DBG_FUNC("Non-blocking read, no data is available!\n"); */
			retval = -EAGAIN;
			goto OUT;
		}

		val = wait_event_interruptible(GPS2_wq, flag2 != 0);
		flag2 = 0;

#if GPS2_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif

		retval = mtk_wcn_stp_receive_data(i_buf2, count, GPSL5_TASK_INDX);

#if GPS2_DEBUG_TRACE_GPIO
		mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif
		/* if we are signaled */
		if (val) {
			if (-ERESTARTSYS == val)
				GPS2_DBG_FUNC("signaled by -ERESTARTSYS(%ld)\n ", val);
			else
				GPS2_DBG_FUNC("signaled by %ld\n ", val);

			break;
		}
	}

#if GPS2_DEBUG_DUMP
	{
		unsigned char *buf_ptr = &i_buf2[0];
		int k = 0;

		pr_info("--[GPS2-READ]--");
		for (k = 0; k < 10; k++) {
			if (k % 16 == 0)
				pr_info("\n");
			pr_info("0x%02x ", i_buf2[k]);
		}
		pr_info("--\n");
	}
#endif

	if (retval > 0) {
		/* we got something from STP driver */
		if (copy_to_user(buf, i_buf2, retval)) {
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
	up(&rd_mtx2);
/*    pr_info("GPS2_read(): retval = %d\n", retval);*/
	return retval;
}

#ifdef GPS_FWCTL_SUPPORT
#ifdef GPS_HW_SUSPEND_SUPPORT
#define HW_SUSPEND_CTRL_TX_LEN2	(2)
#define HW_SUSPEND_CTRL_RX_LEN2	(3)

/* return 0 if okay, otherwise is fail */
static int GPS2_hw_suspend_ctrl(bool to_suspend)
{
	UINT8 tx_buf[HW_SUSPEND_CTRL_TX_LEN2];
	UINT8 rx_buf[HW_SUSPEND_CTRL_RX_LEN2];
	UINT8 rx0 = 0;
	UINT8 rx1 = 0;
	UINT32 tx_len;
	UINT32 rx_len = 0;
	INT32 wmt_status;
	UINT32 local_ms0, local_ms1;
	struct timeval tv;
	UINT64 tmp;

	do_gettimeofday(&tv);
	tmp = local_clock();
	do_div(tmp, 1e6);
	local_ms0 = (UINT32)tmp; /* overflow almost 4.9 days */

	tx_buf[0] = to_suspend ? GPS2_FWCTL_OPCODE_ENTER_STOP_MODE :
		GPS2_FWCTL_OPCODE_EXIT_STOP_MODE;
	tx_len = 1;

	wmt_status = mtk_wmt_gps_mcu_ctrl(&tx_buf[0], tx_len,
		&rx_buf[0], HW_SUSPEND_CTRL_RX_LEN2, &rx_len);

	/* local_ms1 = jiffies_to_msecs(jiffies); */
	tmp = local_clock();
	do_div(tmp, 1e6);
	local_ms1 = (UINT32)tmp;

	if (wmt_status == 0) { /* 0 is okay */
		if (rx_len >= 2) {
			rx0 = rx_buf[0]; /* fw_status, 0 is okay */
			rx1 = rx_buf[1]; /* opcode */
		}
	}

	if (wmt_status != 0 || rx0 != 0) {
		/* Fail due to WMT fail or FW not support,
		 * bypass the following operations
		 */
		GPS2_WARN_FUNC("GPS2_hw_suspend_ctrl %d: st=%d, rx_len=%u ([0]=%u, [1]=%u), ms0=%u, ms1=%u",
			to_suspend, wmt_status, rx_len, rx0, rx1, local_ms0, local_ms1);
		return -1;
	}

	/* Okay */
	GPS2_INFO_FUNC("GPS2_hw_suspend_ctrl %d: st=%d, rx_len=%u ([0]=%u, [1]=%u), ms0=%u, ms1=%u",
		to_suspend, wmt_status, rx_len, rx0, rx1, local_ms0, local_ms1);
	return 0;
}

static int GPS2_hw_suspend(void)
{
	MTK_WCN_BOOL wmt_okay;
	enum gps_ctrl_status_enum gps_status;

	gps_status = g_gps2_ctrl_status;
	if (gps_status == GPS_OPENED) {
		if (GPS2_hw_suspend_ctrl(true) != 0)
			return -EINVAL; /* Stands for not support */

		wmt_okay = mtk_wmt_gps_l5_suspend_ctrl(MTK_WCN_BOOL_TRUE);
		if (!wmt_okay)
			GPS2_WARN_FUNC("mtk_wmt_gps_l5_suspend_ctrl(1), is_ok = %d", wmt_okay);

		/* register event cb will clear GPS STP buffer */
		mtk_wcn_stp_register_event_cb(GPSL5_TASK_INDX, 0x0);
		GPS2_DBG_FUNC("mtk_wcn_stp_register_event_cb to null");

		/* No need to clear the flag2 due to it just a flag2 and not stands for has data
		 * flag2 = 0;
		 *
		 * Keep msgcb due to GPS still need to recevice reset event under HW suspend mode
		 * mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPSL5);
		 */

		GPS_reference_count(HANDLE_DESENSE, false, GPS_USER2);
		gps2_hold_wake_lock(0);
		GPS2_WARN_FUNC("gps2_hold_wake_lock(0)\n");

		GPS2_ctrl_status_change_from_to(GPS_OPENED, GPS_SUSPENDED);
	} else
		GPS2_WARN_FUNC("GPS2_hw_suspend(): status %d not match\n", gps_status);

	return 0;
}

static int GPS2_hw_resume(void)
{
	MTK_WCN_BOOL wmt_okay;
	enum gps_ctrl_status_enum gps_status;

	gps_status = g_gps2_ctrl_status;
	if (gps_status == GPS_SUSPENDED) {
	GPS_reference_count(HANDLE_DESENSE, true, GPS_USER2);
		gps2_hold_wake_lock(1);
		GPS2_WARN_FUNC("gps2_hold_wake_lock(1)\n");

		wmt_okay = mtk_wmt_gps_l5_suspend_ctrl(MTK_WCN_BOOL_FALSE);
		if (!wmt_okay)
			GPS2_WARN_FUNC("mtk_wmt_gps_l5_suspend_ctrl(0), is_ok = %d", wmt_okay);

		if (GPS2_hw_suspend_ctrl(false) != 0)
			return -EINVAL; /* Stands for not support */

		mtk_wcn_stp_register_event_cb(GPSL5_TASK_INDX, GPS2_event_cb);
		GPS2_ctrl_status_change_from_to(GPS_SUSPENDED, GPS_OPENED);
	} else
		GPS2_WARN_FUNC("GPS2_hw_resume(): status %d not match\n", gps_status);

	return 0;
}
#endif /* GPS_HW_SUSPEND_SUPPORT */

#endif /* GPS_FWCTL_SUPPORT */

/* block until wmt reset happen or GPS_close */
static int GPS2_listen_wmt_rst(void)
{
	long val = 0;

	rst_listening_flag2 = 1;
	while (!rst_happened_or_gps_close_flag2) {
		val = wait_event_interruptible(GPS2_rst_wq, rst_happened_or_gps_close_flag2);

		GPS2_WARN_FUNC("GPS2_listen_wmt_rst(): val = %ld, cond = %d, rstflag2 = %d, status = %d\n",
			val, rst_happened_or_gps_close_flag2, rstflag2, g_gps2_ctrl_status);

		/* if we are signaled */
		if (val) {
			rst_listening_flag2 = 0;
			return val;
		}
	}

	rst_listening_flag2 = 0;
	return 0;
}

/* int GPS_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) */
long GPS2_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	#if 0
	ENUM_WMTHWVER_TYPE_T hw_ver_sym = WMTHWVER_INVALID;
	#endif
	UINT32 hw_version = 0;
	UINT32 fw_version = 0;
	UINT32 gps_lna_pin = 0;

	pr_info("GPS2_ioctl(): cmd (%d)\n", cmd);

	switch (cmd) {
	case 0:		/* enable/disable STP */
		GPS2_DBG_FUNC("GPS_ioctl(): disable STP control from GPS2 dev\n");
		retval = -EINVAL;
#if 1
#else
		/* George: STP is controlled by WMT only */
		mtk_wcn_stp_enable(arg);
#endif
		break;

	case 1:		/* send raw data */
		GPS2_DBG_FUNC("GPS2_ioctl(): disable raw data from GPS2 dev\n");
		retval = -EINVAL;
		break;

	#if 0
	case COMBO_IOC_GPS2_HWVER:
		/*get combo hw version */
		hw_ver_sym = mtk_wcn_wmt_hwver_get();

		GPS2_DBG_FUNC("GPS_ioctl(): get hw version = %d, sizeof(hw_ver_sym) = %zd\n",
			      hw_ver_sym, sizeof(hw_ver_sym));
		if (copy_to_user((int __user *)arg, &hw_ver_sym, sizeof(hw_ver_sym)))
			retval = -EFAULT;

		break;
	#endif
	case COMBO_IOC_GPS2_IC_HW_VERSION:
		/*get combo hw version from ic,  without wmt mapping */
		hw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);

		GPS2_DBG_FUNC("GPS2_ioctl(): get hw version = 0x%x\n", hw_version);
		if (copy_to_user((int __user *)arg, &hw_version, sizeof(hw_version)))
			retval = -EFAULT;

		break;

	case COMBO_IOC_GPS2_IC_FW_VERSION:
		/*get combo fw version from ic, without wmt mapping */
		fw_version = mtk_wcn_wmt_ic_info_get(WMTCHIN_FWVER);

		GPS2_DBG_FUNC("GPS2_ioctl(): get fw version = 0x%x\n", fw_version);
		if (copy_to_user((int __user *)arg, &fw_version, sizeof(fw_version)))
			retval = -EFAULT;

		break;
	case COMBO_IOC_RTC_FLAG2:

		retval = rtc_GPS2_low_power_detected();

		GPS2_DBG_FUNC("low power flag2 (%d)\n", retval);
		break;
	case COMBO_IOC_CO_CLOCK_FLAG2:
#if SOC_CO_CLOCK_FLAG
		retval = mtk_wcn_wmt_co_clock_flag_get();
#endif
		GPS2_DBG_FUNC("GPS co_clock_flag (%d)\n", retval);
		break;
	case COMBO_IOC_D1_EFUSE_GET2:
#if defined(CONFIG_MACH_MT6735)
		do {
			char *addr = ioremap(0x10206198, 0x4);

			retval = *(unsigned int *)addr;
			GPS2_DBG_FUNC("D1 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#elif defined(CONFIG_MACH_MT6763)
		do {
			char *addr = ioremap(0x11f10048, 0x4);

			retval = *(unsigned int *)addr;
			GPS2_DBG_FUNC("6763 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#else
		GPS2_ERR_FUNC("Read Efuse not supported in this platform\n");
#endif
		break;

	case COMBO_IOC_TRIGGER_WMT_ASSERT2:
		/* Trigger FW assert for debug */
		GPS2_INFO_FUNC("%s: Host trigger FW assert......, reason:%lu\n", __func__, arg);
		retval = mtk_wcn_wmt_assert(WMTDRV_TYPE_GPSL5, arg);
		if (retval == MTK_WCN_BOOL_TRUE) {
			GPS2_INFO_FUNC("Host trigger FW assert succeed\n");
			retval = 0;
		} else {
			GPS2_ERR_FUNC("Host trigger FW assert Failed\n");
			retval = (-EBUSY);
		}
		break;
	case COMBO_IOC_WMT_STATUS2:
		if (rstflag2 == 1) {
			/* chip resetting */
			retval = -888;
		} else if (rstflag2 == 2) {
			/* chip reset end */
			retval = -889;
			/*
			 * rstflag2 = 0 is cleared by GPS_open or GPS_close,
			 * no need to clear it here
			 */
		} else {
			/* normal */
			retval = 0;
		}
		GPS2_DBG_FUNC("rstflag2(%d), retval(%d)\n", rstflag2, retval);
		break;
	case COMBO_IOC_TAKE_GPS2_WAKELOCK:
		GPS2_INFO_FUNC("Ioctl to take gps2 wakelock\n");
		gps2_hold_wake_lock(1);
		if (wake_lock_acquired2 == 1)
			retval = 0;
		else
			retval = -EAGAIN;
		break;
	case COMBO_IOC_GIVE_GPS2_WAKELOCK:
		GPS2_INFO_FUNC("Ioctl to give gps2 wakelock\n");
		gps2_hold_wake_lock(0);
		if (wake_lock_acquired2 == 0)
			retval = 0;
		else
			retval = -EAGAIN;
		break;

#ifdef GPS_HW_SUSPEND_SUPPORT
	case COMBO_IOC_GPS2_HW_SUSPEND:
		GPS2_INFO_FUNC("COMBO_IOC_GPS2_HW_SUSPEND\n");
		retval = GPS2_hw_suspend();
		break;
	case COMBO_IOC_GPS2_HW_RESUME:
		GPS2_INFO_FUNC("COMBO_IOC_GPS2_HW_RESUME\n");
		retval = GPS2_hw_resume();
		break;
#endif /* GPS_HW_SUSPEND_SUPPORT */

	case COMBO_IOC_GPS2_LISTEN_RST_EVT:
		GPS2_INFO_FUNC("COMBO_IOC_GPS2_LISTEN_RST_EVT\n");
		retval = GPS2_listen_wmt_rst();
		break;

	case COMBO_IOC_GET_GPS2_LNA_PIN:
		gps_lna_pin = mtk_wmt_get_gps_lna_pin_num();
		GPS2_DBG_FUNC("GPS2_ioctl(): get gps lna pin = %d\n", gps_lna_pin);
		if (copy_to_user((int __user *)arg, &gps_lna_pin, sizeof(gps_lna_pin)))
			retval = -EFAULT;
		break;
	default:
		retval = -EFAULT;
		GPS2_DBG_FUNC("GPS2_ioctl(): unknown cmd (%d)\n", cmd);
		break;
	}

/*OUT:*/
	return retval;
}

long GPS2_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	pr_info("%s: cmd (%d)\n", __func__, cmd);
	ret = GPS2_unlocked_ioctl(filp, cmd, arg);
	pr_info("%s: cmd (%d)\n", __func__, cmd);
	return ret;
}

static void gps2_cdev_rst_cb(ENUM_WMTDRV_TYPE_T src,
			    ENUM_WMTDRV_TYPE_T dst, ENUM_WMTMSG_TYPE_T type, void *buf, unsigned int sz)
{

	/* To handle reset procedure please */
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	GPS2_DBG_FUNC("sizeof(ENUM_WMTRSTMSG_TYPE_T) = %zd\n", sizeof(ENUM_WMTRSTMSG_TYPE_T));
	if (sz <= sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		memcpy((char *)&rst_msg, (char *)buf, sz);
		GPS2_DBG_FUNC("src = %d, dst = %d, type = %d, buf = 0x%x sz = %d, max = %d\n", src,
			      dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);

		if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_GPSL5) && (type == WMTMSG_TYPE_RESET)) {
			switch (rst_msg) {
			case WMTRSTMSG_RESET_START:
				GPS2_INFO_FUNC("Whole chip reset start!\n");
				rstflag2 = 1;
#ifdef GPS_FWCTL_SUPPORT
				down(&fwctl_mtx);
				GPS_reference_count(FGGPS_FWCTL_EADY, false, GPS_USER2);
				up(&fwctl_mtx);
#endif
				GPS2_ctrl_status_change_to(GPS_RESET_START);
				break;
			case WMTRSTMSG_RESET_END:
			case WMTRSTMSG_RESET_END_FAIL:
				if (rst_msg == WMTRSTMSG_RESET_END)
					GPS2_INFO_FUNC("Whole chip reset end!\n");
				else
					GPS2_INFO_FUNC("Whole chip reset fail!\n");
				rstflag2 = 2;

				GPS2_ctrl_status_change_to(GPS_RESET_DONE);
				break;
			default:
				break;
			}
		}
	} else {
		/*message format invalid */
		GPS2_WARN_FUNC("Invalid message format!\n");
	}
}

static int GPS2_open(struct inode *inode, struct file *file)
{
	pr_info("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;
	if (rstflag2 == 1) {
		GPS2_WARN_FUNC("whole chip resetting...\n");
		return -EPERM;
	}

#if 1				/* GeorgeKuo: turn on function before check stp ready */
	/* turn on BT */
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_pin_ctrl(GPS_DATA_LINK_ID1, true, false);
#endif
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPSL5) == MTK_WCN_BOOL_FALSE) {
		GPS2_WARN_FUNC("WMT turn on GPS2 fail!\n");
		return -ENODEV;
	}

	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_GPSL5, gps2_cdev_rst_cb);
	GPS2_WARN_FUNC("WMT turn on GPS2 OK!\n");
	rstflag2 = 0;

#endif

	if (mtk_wcn_stp_is_ready()) {
#if 0
		if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPSL5) == MTK_WCN_BOOL_FALSE) {
			GPS2_WARN_FUNC("WMT turn on GPS fail!\n");
			return -ENODEV;
		}
		GPS2_DBG_FUNC("WMT turn on GPS2 OK!\n");
#endif
		mtk_wcn_stp_register_event_cb(GPSL5_TASK_INDX, GPS2_event_cb);
	} else {
		GPS2_ERR_FUNC("STP is not ready, Cannot open GPS2 Devices\n\r");

		/*return error code */
		return -ENODEV;
	}
	gps2_hold_wake_lock(1);
	GPS2_WARN_FUNC("gps2_hold_wake_lock(1)\n");

	GPS_reference_count(HANDLE_DESENSE, true, GPS_USER2);

#ifdef GPS_FWCTL_SUPPORT
	down(&fwctl_mtx);
	GPS_reference_count(FGGPS_FWCTL_EADY, true, GPS_USER2);
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	if (fgGps_fwlog_on) {
		/* GPS fw clear log on flag2 when GPS on, no need to send it if log setting is off */
		GPS_reference_count(FWLOG_CTRL_INNER, fgGps_fwlog_on, GPS_USER2);
	}
#endif
	up(&fwctl_mtx);
#endif /* GPS_FWCTL_SUPPORT */

	rst_happened_or_gps_close_flag2 = 0;
	GPS2_ctrl_status_change_to(GPS_OPENED);
	return 0;
}

static int GPS2_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	pr_info("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;

	if (rstflag2 == 1) {
		GPS2_WARN_FUNC("whole chip resetting...\n");
		ret = -EPERM;
		goto _out;
	}

#ifdef GPS_FWCTL_SUPPORT
	down(&fwctl_mtx);
	GPS_reference_count(FGGPS_FWCTL_EADY, false, GPS_USER2);
	up(&fwctl_mtx);
#endif
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_pin_ctrl(GPS_DATA_LINK_ID1, false, false);
#endif
	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_GPSL5) == MTK_WCN_BOOL_FALSE) {
		GPS2_WARN_FUNC("WMT turn off GPS2 fail!\n");
		ret = -EIO;
		goto _out;
	}

	GPS2_WARN_FUNC("WMT turn off GPS2 OK!\n");
	rstflag2 = 0;
	/*Flush Rx Queue */
	mtk_wcn_stp_register_event_cb(GPSL5_TASK_INDX, 0x0);	/* unregister event callback function */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPSL5);

_out:
	gps2_hold_wake_lock(0);
	GPS2_WARN_FUNC("gps2_hold_wake_lock(0)\n");

	GPS_reference_count(HANDLE_DESENSE, false, GPS_USER2);

	GPS2_ctrl_status_change_to(GPS_CLOSED);
	return ret;
}

const struct file_operations GPS2_fops = {
	.open = GPS2_open,
	.release = GPS2_close,
	.read = GPS2_read,
	.write = GPS2_write,
/* .ioctl = GPS_ioctl */
	.unlocked_ioctl = GPS2_unlocked_ioctl,
	.compat_ioctl = GPS2_compat_ioctl,
};

void GPS2_event_cb(void)
{
/*    pr_debug("GPS2_event_cb()\n");*/

	flag2 = 1;
	wake_up(&GPS2_wq);
}

