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
#include <linux/version.h>

#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
#include "gps_lna_drv.h"
#endif
MODULE_LICENSE("GPL");

#define GPS_DRIVER_NAME "mtk_stp_GPS_chrdev"
#define GPS2_DRIVER_NAME "mtk_stp_GPS2_chrdev"

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
#define COMBO_IOC_D1_EFUSE_GET        9
#define COMBO_IOC_RTC_FLAG           10
#define COMBO_IOC_CO_CLOCK_FLAG      11
#define COMBO_IOC_TRIGGER_WMT_ASSERT 12
#define COMBO_IOC_WMT_STATUS         13
#define COMBO_IOC_TAKE_GPS_WAKELOCK  14
#define COMBO_IOC_GIVE_GPS_WAKELOCK  15
#define COMBO_IOC_GET_GPS_LNA_PIN    16
#define COMBO_IOC_GPS_FWCTL          17
#define COMBO_IOC_GPS_HW_SUSPEND     18
#define COMBO_IOC_GPS_HW_RESUME      19
#define COMBO_IOC_GPS_LISTEN_RST_EVT 20

static UINT32 gDbgLevel = GPS_LOG_DBG;

#define GPS_DBG_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_debug(PFX "[D]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS_INFO_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_INFO)	\
		pr_info(PFX "[I]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS_WARN_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_WARN)	\
		pr_warn(PFX "[W]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS_ERR_FUNC(fmt, arg...)	\
do { if (gDbgLevel >= GPS_LOG_ERR)	\
		pr_err(PFX "[E]%s: "  fmt, __func__, ##arg);	\
} while (0)
#define GPS_TRC_FUNC(f)	\
do { if (gDbgLevel >= GPS_LOG_DBG)	\
		pr_info(PFX "<%s> <%d>\n", __func__, __LINE__);	\
} while (0)

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
bool fgGps_fwlog_on;
#endif

#ifdef GPS_FWCTL_SUPPORT
bool fgGps_fwctl_ready;
#endif

static int GPS_devs = 1;	/* device count */
static int GPS_major = GPS_DEV_MAJOR;	/* dynamic allocation */
module_param(GPS_major, uint, 0);
static struct cdev GPS_cdev;
#ifdef CONFIG_GPSL5_SUPPORT
static int GPS2_devs = 2;	/* device count */
static struct cdev GPS2_cdev;
#endif

static struct wakeup_source *gps_wake_lock_ptr;
const char gps_wake_lock_name[] = "gpswakelock";
static unsigned char wake_lock_acquired;   /* default: 0 */

#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_ENG_BUILD))
#define STP_GPS_BUFFER_SIZE 2048
#else
#define STP_GPS_BUFFER_SIZE MTKSTP_BUFFER_SIZE
#endif
static unsigned char i_buf[STP_GPS_BUFFER_SIZE];	/* input buffer of read() */
static unsigned char o_buf[STP_GPS_BUFFER_SIZE];	/* output buffer of write() */
static struct semaphore wr_mtx, rd_mtx, status_mtx;
struct semaphore fwctl_mtx;

static DECLARE_WAIT_QUEUE_HEAD(GPS_wq);
static DECLARE_WAIT_QUEUE_HEAD(GPS_rst_wq);
static int flag;
static int rstflag;
static int rst_listening_flag;
static int rst_happened_or_gps_close_flag;

static enum gps_ctrl_status_enum g_gps_ctrl_status;

static void GPS_check_and_wakeup_rst_listener(enum gps_ctrl_status_enum to)
{
	if (to == GPS_RESET_START || to == GPS_RESET_DONE  || to == GPS_CLOSED) {
		rst_happened_or_gps_close_flag = 1;
		if (rst_listening_flag) {
			wake_up(&GPS_rst_wq);
			GPS_WARN_FUNC("Set GPS rst_happened_or_gps_close_flag because to = %d", to);
		}
	}
}

#ifdef GPS_HW_SUSPEND_SUPPORT
static void GPS_ctrl_status_change_from_to(enum gps_ctrl_status_enum from, enum gps_ctrl_status_enum to)
{
	bool okay = true;
	enum gps_ctrl_status_enum status_backup;

	down(&status_mtx);
	/* Due to checking status and setting status are not atomic,
	 * mutex is needed to make sure they are an atomic operation.
	 * Note: reading the status is no need to protect
	 */
	status_backup = g_gps_ctrl_status;
	if (status_backup == from)
		g_gps_ctrl_status = to;
	else
		okay = false;
	up(&status_mtx);

	if (!okay)
		GPS_WARN_FUNC("GPS unexpected status %d, not chagne from %d to %d", status_backup, from, to);
	else
		GPS_check_and_wakeup_rst_listener(to);
}
#endif

static enum gps_ctrl_status_enum GPS_ctrl_status_change_to(enum gps_ctrl_status_enum to)
{
	enum gps_ctrl_status_enum status_backup;

	down(&status_mtx);
	status_backup = g_gps_ctrl_status;
	g_gps_ctrl_status = to;
	up(&status_mtx);

	GPS_check_and_wakeup_rst_listener(to);

	return status_backup;
}

static void GPS_event_cb(void);

static void gps_hold_wake_lock(int hold)
{
	if (hold == 1) {
		if (!wake_lock_acquired) {
			GPS_DBG_FUNC("acquire gps wake_lock acquired = %d\n", wake_lock_acquired);
			__pm_stay_awake(gps_wake_lock_ptr);
			wake_lock_acquired = 1;
		} else {
			GPS_DBG_FUNC("acquire gps wake_lock acquired = %d (do nothing)\n", wake_lock_acquired);
		}
	} else if (hold == 0) {
		if (wake_lock_acquired) {
			GPS_DBG_FUNC("release gps wake_lock acquired = %d\n", wake_lock_acquired);
			__pm_relax(gps_wake_lock_ptr);
			wake_lock_acquired = 0;
		} else {
			GPS_DBG_FUNC("release gps wake_lock acquired = %d (do nothing)\n", wake_lock_acquired);
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
		int copy_size = (count < STP_GPS_BUFFER_SIZE) ? count : STP_GPS_BUFFER_SIZE;

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
		if (written == 0) {
			retval = -ENOSPC;
			/* no windowspace in STP is available, */
			/* native process should not call GPS_write with no delay at all */
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

    /* pr_debug("GPS_read(): count %d pos %lld\n", count, *f_pos); */
	if (rstflag == 1) {
		if (filp->f_flags & O_NONBLOCK) {
			/* GPS_DBG_FUNC("Non-blocking read, whole chip reset occurs! rstflag=%d\n", rstflag); */
			retval = -EIO;
			goto OUT;
		}
	}

	if (count > STP_GPS_BUFFER_SIZE)
		count = STP_GPS_BUFFER_SIZE;

#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_LOW);
#endif
	retval = mtk_wcn_stp_receive_data(i_buf, count, GPS_TASK_INDX);
#if GPS_DEBUG_TRACE_GPIO
	mtk_wcn_stp_debug_gpio_assert(IDX_GPS_RX, DBG_TIE_HIGH);
#endif

	while (retval == 0) {
		/* got nothing, wait for STP's signal */
		/* wait_event(GPS_wq, flag != 0); *//* George: let signal wake up */
		if (filp->f_flags & O_NONBLOCK) {
			/* GPS_DBG_FUNC("Non-blocking read, no data is available!\n"); */
			retval = -EAGAIN;
			goto OUT;
		}

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

#ifdef GPS_FWCTL_SUPPORT
#ifdef GPS_FWCTL_IOCTL_SUPPORT
struct gps_fwctl_data {
	UINT32 size;
	UINT32 tx_len;
	UINT32 rx_max;
	UINT32 pad;         /* 128byte */
	UINT64 p_tx_buf;    /* cast pointer to UINT64 to be compatible both 32/64bit */
	UINT64 p_rx_buf;    /* 256byte */
	UINT64 p_rx_len;
	UINT64 p_reserved;  /* 384byte */
};

#define GPS_FWCTL_BUF_MAX   (128)
long GPS_fwctl(struct gps_fwctl_data *user_ptr)
{
	UINT8 tx_buf[GPS_FWCTL_BUF_MAX];
	UINT8 rx_buf[GPS_FWCTL_BUF_MAX];
	UINT8 tx0 = 0;
	UINT8 rx0 = 0;
	UINT8 rx1 = 0;
	UINT32 rx_len = 0;
	UINT32 rx_to_user_len;
	struct gps_fwctl_data ctl_data;
	INT32 status = 0;
	UINT64 delta_time = 0;
	bool fwctl_ready;
	int retval = 0;

	if (copy_from_user(&ctl_data, user_ptr, sizeof(struct gps_fwctl_data))) {
		pr_err("GPS_fwctl: copy_from_user error - ctl_data");
		return -EFAULT;
	}

	if (ctl_data.size < sizeof(struct gps_fwctl_data)) {
		/* user space data size not enough, risk to use it */
		pr_err("GPS_fwctl: struct size(%u) is too short than(%u)",
			ctl_data.size, (UINT32)sizeof(struct gps_fwctl_data));
		return -EFAULT;
	}

	if ((ctl_data.tx_len > GPS_FWCTL_BUF_MAX) || (ctl_data.tx_len == 0)) {
		/* check tx data len */
		pr_err("GPS_fwctl: tx_len=%u too long (> %u) or too short",
			ctl_data.tx_len, GPS_FWCTL_BUF_MAX);
		return -EFAULT;
	}

	if (copy_from_user(&tx_buf[0], (PUINT8)ctl_data.p_tx_buf, ctl_data.tx_len)) {
		pr_err("GPS_fwctl: copy_from_user error - tx_buf");
		return -EFAULT;
	}

	down(&fwctl_mtx);
	if (fgGps_fwctl_ready) {
		delta_time = local_clock();
		status = mtk_wmt_gps_mcu_ctrl(
			&tx_buf[0], ctl_data.tx_len, &rx_buf[0], GPS_FWCTL_BUF_MAX, &rx_len);
		delta_time = local_clock() - delta_time;
		do_div(delta_time, 1e3); /* convert to us */
		fwctl_ready = true;
	} else {
		fwctl_ready = false;
	}
	up(&fwctl_mtx);

	tx0 = tx_buf[0];
	if (fwctl_ready && (status == 0) && (rx_len <= GPS_FWCTL_BUF_MAX) && (rx_len >= 2)) {
		rx0 = rx_buf[0];
		rx1 = rx_buf[1];
		pr_info("GPS_fwctl: st=%d, tx_len=%u ([0]=%u), rx_len=%u ([0]=%u, [1]=%u), us=%u",
			status, ctl_data.tx_len, tx0, rx_len, rx0, rx1, (UINT32)delta_time);

		if (ctl_data.rx_max < rx_len)
			rx_to_user_len = ctl_data.rx_max;
		else
			rx_to_user_len = rx_len;

		if (copy_to_user((PUINT8)ctl_data.p_rx_buf, &rx_buf[0], rx_to_user_len)) {
			pr_err("GPS_fwctl: copy_to_user error - rx_buf");
			retval = -EFAULT;
		}

		if (copy_to_user((PUINT32)ctl_data.p_rx_len, &rx_len, sizeof(UINT32))) {
			pr_err("GPS_fwctl: copy_to_user error - rx_len");
			retval = -EFAULT;
		}
		return retval;
	}

	pr_info("GPS_fwctl: st=%d, tx_len=%u ([0]=%u), rx_len=%u, us=%u, ready=%u",
		status, ctl_data.tx_len, tx0, rx_len, (UINT32)delta_time, (UINT32)fwctl_ready);
	return -EFAULT;
}
#endif /* GPS_FWCTL_IOCTL_SUPPORT */

#define GPS_FWLOG_CTRL_BUF_MAX  (20)

#define GPS_NSEC_IN_MSEC (1000000)
UINT64 GPS_get_local_clock_ms(void)
{
	UINT64 tmp;

	/* tmp is ns */
	tmp = local_clock();

	/* tmp is changed to ms after */
	do_div(tmp, GPS_NSEC_IN_MSEC);

	return tmp;
}

void GPS_fwlog_ctrl_inner(bool on)
{
	UINT8 tx_buf[GPS_FWLOG_CTRL_BUF_MAX];
	UINT8 rx_buf[GPS_FWLOG_CTRL_BUF_MAX];
	UINT8 rx0 = 0;
	UINT8 rx1 = 0;
	UINT32 tx_len;
	UINT32 rx_len = 0;
	INT32 status;
	UINT32 fw_tick = 0;
	UINT32 local_ms0, local_ms1;
	struct timeval tv;

	do_gettimeofday(&tv);

	/* 32bit ms overflow almost 4.9 days, it's enough here */
	local_ms0 = (UINT32)GPS_get_local_clock_ms();

	tx_buf[0]  = GPS_FWCTL_OPCODE_LOG_CFG;
	tx_buf[1]  = (UINT8)on;
	tx_buf[2]  = 0;
	tx_buf[3]  = 0;
	tx_buf[4]  = 0; /* bitmask, reserved now */
	tx_buf[5]  = (UINT8)((local_ms0  >>  0) & 0xFF);
	tx_buf[6]  = (UINT8)((local_ms0  >>  8) & 0xFF);
	tx_buf[7]  = (UINT8)((local_ms0  >> 16) & 0xFF);
	tx_buf[8]  = (UINT8)((local_ms0  >> 24) & 0xFF); /* local time msecs */
	tx_buf[9]  = (UINT8)((tv.tv_usec >>  0) & 0xFF);
	tx_buf[10] = (UINT8)((tv.tv_usec >>  8) & 0xFF);
	tx_buf[11] = (UINT8)((tv.tv_usec >> 16) & 0xFF);
	tx_buf[12] = (UINT8)((tv.tv_usec >> 24) & 0xFF); /* utc usec */
	tx_buf[13] = (UINT8)((tv.tv_sec  >>  0) & 0xFF);
	tx_buf[14] = (UINT8)((tv.tv_sec  >>  8) & 0xFF);
	tx_buf[15] = (UINT8)((tv.tv_sec  >> 16) & 0xFF);
	tx_buf[16] = (UINT8)((tv.tv_sec  >> 24) & 0xFF); /* utc sec */
	tx_len = 17;

	status = mtk_wmt_gps_mcu_ctrl(&tx_buf[0], tx_len, &rx_buf[0], GPS_FWLOG_CTRL_BUF_MAX, &rx_len);

	/* local_ms1 = jiffies_to_msecs(jiffies); */
	local_ms1 = (UINT32)GPS_get_local_clock_ms();

	if (status == 0) {
		if (rx_len >= 2) {
			rx0 = rx_buf[0];
			rx1 = rx_buf[1];
		}

		if (rx_len >= 6) {
			fw_tick |= (((UINT32)rx_buf[2]) <<  0);
			fw_tick |= (((UINT32)rx_buf[3]) <<  8);
			fw_tick |= (((UINT32)rx_buf[4]) << 16);
			fw_tick |= (((UINT32)rx_buf[5]) << 24);
		}
	}

	pr_info("GPS_fwlog: st=%d, rx_len=%u ([0]=%u, [1]=%u), ms0=%u, ms1=%u, fw_tick=%u",
		status, rx_len, rx0, rx1, local_ms0, local_ms1, fw_tick);
}

#ifdef GPS_HW_SUSPEND_SUPPORT
#define HW_SUSPEND_CTRL_TX_LEN	(3)
#define HW_SUSPEND_CTRL_RX_LEN	(3)

/* return 0 if okay, otherwise is fail */
static int GPS_hw_suspend_ctrl(bool to_suspend, UINT8 mode)
{
	UINT8 tx_buf[HW_SUSPEND_CTRL_TX_LEN] = {0};
	UINT8 rx_buf[HW_SUSPEND_CTRL_RX_LEN] = {0};
	UINT8 rx0 = 0;
	UINT8 rx1 = 0;
	UINT32 tx_len;
	UINT32 rx_len = 0;
	INT32 wmt_status;
	UINT32 local_ms0, local_ms1;
	struct timeval tv;

	do_gettimeofday(&tv);

	/* 32bit ms overflow almost 4.9 days, it's enough here */
	local_ms0 = (UINT32)GPS_get_local_clock_ms();

	tx_buf[0] = to_suspend ? GPS_FWCTL_OPCODE_ENTER_STOP_MODE :
		GPS_FWCTL_OPCODE_EXIT_STOP_MODE;
	tx_buf[1] = mode;  /*mode value should be set in mnld, 0: HW suspend mode, 1: clock extension mode*/
	tx_len = 2;

	wmt_status = mtk_wmt_gps_mcu_ctrl(&tx_buf[0], tx_len,
		&rx_buf[0], HW_SUSPEND_CTRL_RX_LEN, &rx_len);

	/* local_ms1 = jiffies_to_msecs(jiffies); */
	local_ms1 = (UINT32)GPS_get_local_clock_ms();

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
		GPS_WARN_FUNC("GPS_hw_suspend_ctrl %d: st=%d, rx_len=%u ([0]=%u, [1]=%u), ms0=%u, ms1=%u, mode=%u",
			to_suspend, wmt_status, rx_len, rx0, rx1, local_ms0, local_ms1, mode);
		return -1;
	}

	/* Okay */
	GPS_INFO_FUNC("GPS_hw_suspend_ctrl %d: st=%d, rx_len=%u ([0]=%u, [1]=%u), ms0=%u, ms1=%u, mode=%u",
		to_suspend, wmt_status, rx_len, rx0, rx1, local_ms0, local_ms1, mode);
	return 0;
}

static int GPS_hw_suspend(UINT8 mode)
{
	MTK_WCN_BOOL wmt_okay;
	enum gps_ctrl_status_enum gps_status;

	gps_status = g_gps_ctrl_status;
	if (gps_status == GPS_OPENED) {
		if (GPS_hw_suspend_ctrl(true, mode) != 0)
			return -EINVAL; /* Stands for not support */

#ifdef CONFIG_GPSL5_SUPPORT
		wmt_okay = mtk_wmt_gps_l1_suspend_ctrl(MTK_WCN_BOOL_TRUE);
#else
		wmt_okay = mtk_wmt_gps_suspend_ctrl(MTK_WCN_BOOL_TRUE);
#endif
		if (!wmt_okay)
			GPS_WARN_FUNC("mtk_wmt_gps_l1_suspend_ctrl(1), is_ok = %d", wmt_okay);

		/* register event cb will clear GPS STP buffer */
		mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, 0x0);
		GPS_DBG_FUNC("mtk_wcn_stp_register_event_cb to null");

		/* No need to clear the flag due to it just a flag and not stands for has data
		 * flag = 0;
		 *
		 * Keep msgcb due to GPS still need to recevice reset event under HW suspend mode
		 * mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPS);
		 */

		GPS_reference_count(HANDLE_DESENSE, false, GPS_USER1);
		gps_hold_wake_lock(0);
		GPS_WARN_FUNC("gps_hold_wake_lock(0)\n");

		GPS_ctrl_status_change_from_to(GPS_OPENED, GPS_SUSPENDED);
	} else
		GPS_WARN_FUNC("GPS_hw_suspend(): status %d not match\n", gps_status);

	return 0;
}

static int GPS_hw_resume(UINT8 mode)
{
	MTK_WCN_BOOL wmt_okay;
	enum gps_ctrl_status_enum gps_status;

	gps_status = g_gps_ctrl_status;
	if (gps_status == GPS_SUSPENDED) {
		GPS_reference_count(HANDLE_DESENSE, true, GPS_USER1);
		gps_hold_wake_lock(1);
		GPS_WARN_FUNC("gps_hold_wake_lock(1)\n");

#ifdef CONFIG_GPSL5_SUPPORT
		wmt_okay = mtk_wmt_gps_l1_suspend_ctrl(MTK_WCN_BOOL_FALSE);
#else
		wmt_okay = mtk_wmt_gps_suspend_ctrl(MTK_WCN_BOOL_FALSE);
#endif
		if (!wmt_okay)
			GPS_WARN_FUNC("mtk_wmt_gps_l1_suspend_ctrl(0), is_ok = %d", wmt_okay);

		/* should register it before real resuming to prepare for receiving data */
		mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, GPS_event_cb);
		if (GPS_hw_suspend_ctrl(false, mode) != 0)  /*Ignore mode value for resume stage*/
			return -EINVAL; /* Stands for not support */

		GPS_ctrl_status_change_from_to(GPS_SUSPENDED, GPS_OPENED);
	} else
		GPS_WARN_FUNC("GPS_hw_resume(): status %d not match\n", gps_status);

	return 0;
}
#endif /* GPS_HW_SUSPEND_SUPPORT */

#endif /* GPS_FWCTL_SUPPORT */

void GPS_fwlog_ctrl(bool on)
{
#if (defined(GPS_FWCTL_SUPPORT) && defined(CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH))
	down(&fwctl_mtx);
	if (fgGps_fwctl_ready)
		GPS_reference_count(FWLOG_CTRL_INNER, on, GPS_USER1);
	fgGps_fwlog_on = on;
	up(&fwctl_mtx);
#endif
}

/* block until wmt reset happen or GPS_close */
static int GPS_listen_wmt_rst(void)
{
	long val = 0;

	rst_listening_flag = 1;
	while (!rst_happened_or_gps_close_flag) {
		val = wait_event_interruptible(GPS_rst_wq, rst_happened_or_gps_close_flag);

		GPS_WARN_FUNC("GPS_listen_wmt_rst(): val = %ld, cond = %d, rstflag = %d, status = %d\n",
			val, rst_happened_or_gps_close_flag, rstflag, g_gps_ctrl_status);

		/* if we are signaled */
		if (val) {
			rst_listening_flag = 0;
			return val;
		}
	}

	rst_listening_flag = 0;
	return 0;
}

/* int GPS_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) */
long GPS_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	#if 0
	ENUM_WMTHWVER_TYPE_T hw_ver_sym = WMTHWVER_INVALID;
	#endif
	UINT32 hw_version = 0;
	UINT32 fw_version = 0;
	UINT32 gps_lna_pin = 0;

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

	#if 0
	case COMBO_IOC_GPS_HWVER:
		/*get combo hw version */
		hw_ver_sym = mtk_wcn_wmt_hwver_get();

		GPS_DBG_FUNC("GPS_ioctl(): get hw version = %d, sizeof(hw_ver_sym) = %zd\n",
			      hw_ver_sym, sizeof(hw_ver_sym));
		if (copy_to_user((int __user *)arg, &hw_ver_sym, sizeof(hw_ver_sym)))
			retval = -EFAULT;

		break;
	#endif
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
#if defined(CONFIG_MACH_MT6735)
		do {
			char *addr = ioremap(0x10206198, 0x4);

			retval = *(volatile unsigned int *)addr;
			GPS_DBG_FUNC("D1 efuse (0x%x)\n", retval);
			iounmap(addr);
		} while (0);
#elif defined(CONFIG_MACH_MT6763)
		do {
			char *addr = ioremap(0x11f10048, 0x4);

			retval = *(volatile unsigned int *)addr;
			GPS_DBG_FUNC("bianco efuse (0x%x)\n", retval);
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
	case COMBO_IOC_WMT_STATUS:
		if (rstflag == 1) {
			/* chip resetting */
			retval = -888;
		} else if (rstflag == 2) {
			/* chip reset end */
			retval = -889;
			/*
			 * rstflag = 0 is cleared by GPS_open or GPS_close,
			 * no need to clear it here
			 */
		} else {
			/* normal */
			retval = 0;
		}
		GPS_DBG_FUNC("rstflag(%d), retval(%d)\n", rstflag, retval);
		break;
	case COMBO_IOC_TAKE_GPS_WAKELOCK:
		GPS_INFO_FUNC("Ioctl to take gps wakelock\n");
		gps_hold_wake_lock(1);
		if (wake_lock_acquired == 1)
			retval = 0;
		else
			retval = -EAGAIN;
		break;
	case COMBO_IOC_GIVE_GPS_WAKELOCK:
		GPS_INFO_FUNC("Ioctl to give gps wakelock\n");
		gps_hold_wake_lock(0);
		if (wake_lock_acquired == 0)
			retval = 0;
		else
			retval = -EAGAIN;
		break;
#ifdef GPS_FWCTL_IOCTL_SUPPORT
	case COMBO_IOC_GPS_FWCTL:
		GPS_INFO_FUNC("COMBO_IOC_GPS_FWCTL\n");
		retval = GPS_fwctl((struct gps_fwctl_data *)arg);
		break;
#endif

#ifdef GPS_HW_SUSPEND_SUPPORT
	case COMBO_IOC_GPS_HW_SUSPEND:
		GPS_INFO_FUNC("COMBO_IOC_GPS_HW_SUSPEND: mode %lu\n", arg);
		retval = GPS_hw_suspend((UINT8)(arg&0xFF));
		break;
	case COMBO_IOC_GPS_HW_RESUME:
		GPS_INFO_FUNC("COMBO_IOC_GPS_HW_RESUME: mode %lu\n", arg);
		retval = GPS_hw_resume((UINT8)(arg&0xFF));
		break;
#endif /* GPS_HW_SUSPEND_SUPPORT */

	case COMBO_IOC_GPS_LISTEN_RST_EVT:
		GPS_INFO_FUNC("COMBO_IOC_GPS_LISTEN_RST_EVT\n");
		retval = GPS_listen_wmt_rst();
		break;

	case COMBO_IOC_GET_GPS_LNA_PIN:
		gps_lna_pin = mtk_wmt_get_gps_lna_pin_num();
		GPS_DBG_FUNC("GPS_ioctl(): get gps lna pin = %d\n", gps_lna_pin);
		if (copy_to_user((int __user *)arg, &gps_lna_pin, sizeof(gps_lna_pin)))
			retval = -EFAULT;
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

	/* To handle reset procedure please */
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	GPS_DBG_FUNC("sizeof(ENUM_WMTRSTMSG_TYPE_T) = %zd\n", sizeof(ENUM_WMTRSTMSG_TYPE_T));
	if (sz <= sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		memcpy((char *)&rst_msg, (char *)buf, sz);
		GPS_DBG_FUNC("src = %d, dst = %d, type = %d, buf = 0x%x sz = %d, max = %d\n", src,
			      dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);

		if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_GPS) && (type == WMTMSG_TYPE_RESET)) {
			switch (rst_msg) {
			case WMTRSTMSG_RESET_START:
				GPS_INFO_FUNC("Whole chip reset start!\n");
				rstflag = 1;
#ifdef GPS_FWCTL_SUPPORT
				down(&fwctl_mtx);
				GPS_reference_count(FGGPS_FWCTL_EADY, false, GPS_USER1);
				up(&fwctl_mtx);
#endif
				GPS_ctrl_status_change_to(GPS_RESET_START);
				break;
			case WMTRSTMSG_RESET_END:
			case WMTRSTMSG_RESET_END_FAIL:
				if (rst_msg == WMTRSTMSG_RESET_END)
					GPS_INFO_FUNC("Whole chip reset end!\n");
				else
					GPS_INFO_FUNC("Whole chip reset fail!\n");
				rstflag = 2;

				GPS_ctrl_status_change_to(GPS_RESET_DONE);
				break;
			default:
				break;
			}
		}
	} else {
		/*message format invalid */
		GPS_WARN_FUNC("Invalid message format!\n");
	}
}

static bool desense_handled_flag;
static void GPS_handle_desense(bool on)
{
	bool to_do = false, handled = false;

	down(&status_mtx);
	handled = desense_handled_flag;
	if ((on && !handled) || (!on && handled))
		to_do = true;
	else
		to_do = false;

	if (on)
		desense_handled_flag = true;
	else
		desense_handled_flag = false;
	up(&status_mtx);

	if (!to_do) {
		GPS_WARN_FUNC("GPS_handle_desense(%d) not to go due to handled = %d", on, handled);
		return;
	}

	if (on) {
#if defined(CONFIG_MACH_MT6739)
		if (freqhopping_config(FH_MEM_PLLID, 0, 1) == 0)
			GPS_WARN_FUNC("Enable MEMPLL successfully\n");
		else
			GPS_WARN_FUNC("Error to enable MEMPLL\n");
#endif
#if defined(CONFIG_MACH_MT6580)
		GPS_WARN_FUNC("export_clk_buf: %x\n", CLK_BUF_AUDIO);
		KERNEL_clk_buf_ctrl(CLK_BUF_AUDIO, 1);
#endif
#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761)
		dvfsrc_enable_dvfs_freq_hopping(1);
		GPS_WARN_FUNC("mt6765/61 GPS desense solution on\n");
#endif
	} else {
#if defined(CONFIG_MACH_MT6739)
		if (freqhopping_config(FH_MEM_PLLID, 0, 0) == 0)
			GPS_WARN_FUNC("disable MEMPLL successfully\n");
		else
			GPS_WARN_FUNC("Error to disable MEMPLL\n");
#endif
#if defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6761)
		dvfsrc_enable_dvfs_freq_hopping(0);
		GPS_WARN_FUNC("mt6765/61 GPS desense solution off\n");
#endif
#if defined(CONFIG_MACH_MT6580)
		GPS_WARN_FUNC("export_clk_buf: %x\n", CLK_BUF_AUDIO);
		KERNEL_clk_buf_ctrl(CLK_BUF_AUDIO, 0);
#endif
	}
}

static int GPS_open(struct inode *inode, struct file *file)
{
	pr_warn("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;
	if (rstflag == 1) {
		GPS_WARN_FUNC("whole chip resetting...\n");
		return -EPERM;
	}

#if 1				/* GeorgeKuo: turn on function before check stp ready */
	/* turn on BT */
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_pin_ctrl(GPS_DATA_LINK_ID0, true, false);
#endif
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS) == MTK_WCN_BOOL_FALSE) {
		GPS_WARN_FUNC("WMT turn on GPS fail!\n");
		return -ENODEV;
	}

	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_GPS, gps_cdev_rst_cb);
	GPS_WARN_FUNC("WMT turn on GPS OK!\n");
	rstflag = 0;

#endif

	if (mtk_wcn_stp_is_ready()) {
#if 0
		if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_GPS) == MTK_WCN_BOOL_FALSE) {
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
	GPS_WARN_FUNC("gps_hold_wake_lock(1)\n");

	GPS_reference_count(HANDLE_DESENSE, true, GPS_USER1);

#ifdef GPS_FWCTL_SUPPORT
	down(&fwctl_mtx);
	GPS_reference_count(FGGPS_FWCTL_EADY, true, GPS_USER1);
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	if (fgGps_fwlog_on) {
		/* GPS fw clear log on flag when GPS on, no need to send it if log setting is off */
		GPS_reference_count(FWLOG_CTRL_INNER, fgGps_fwlog_on, GPS_USER1);
	}
#endif
	up(&fwctl_mtx);
#endif /* GPS_FWCTL_SUPPORT */

	rst_happened_or_gps_close_flag = 0;
	GPS_ctrl_status_change_to(GPS_OPENED);
	return 0;
}

static int GPS_close(struct inode *inode, struct file *file)
{
	int ret = 0;
	pr_warn("%s: major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;

	if (rstflag == 1) {
		GPS_WARN_FUNC("whole chip resetting...\n");
		ret = -EPERM;
		goto _out;
	}

#ifdef GPS_FWCTL_SUPPORT
	down(&fwctl_mtx);
	GPS_reference_count(FGGPS_FWCTL_EADY, false, GPS_USER1);
	up(&fwctl_mtx);
#endif
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_pin_ctrl(GPS_DATA_LINK_ID0, false, false);
#endif
	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_GPS) == MTK_WCN_BOOL_FALSE) {
		GPS_WARN_FUNC("WMT turn off GPS fail!\n");
		ret = -EIO;	/* mostly, native programer does not care this return vlaue, */
				/* but we still return error code. */
		goto _out;
	}
	GPS_WARN_FUNC("WMT turn off GPS OK!\n");
	rstflag = 0;
	/*Flush Rx Queue */
	mtk_wcn_stp_register_event_cb(GPS_TASK_INDX, 0x0);	/* unregister event callback function */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_GPS);

_out:
	gps_hold_wake_lock(0);
	GPS_WARN_FUNC("gps_hold_wake_lock(0)\n");

	GPS_reference_count(HANDLE_DESENSE, false, GPS_USER1);

	GPS_ctrl_status_change_to(GPS_CLOSED);
	return ret;
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
struct class *stpgps_class;
#ifdef CONFIG_GPSL5_SUPPORT
struct class *stpgps2_class;
#endif
#endif

static int GPS_init(void)
{
	dev_t dev = MKDEV(GPS_major, 0);
	int cdev_err = 0;
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	struct device *stpgps_dev = NULL;
#endif
#ifdef CONFIG_GPSL5_SUPPORT
	dev_t dev2 = MKDEV(GPS_major, 1);
	int cdev2_err = 0;
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	struct device *stpgps2_dev = NULL;
#endif
#endif
	int alloc_ret = 0;


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

#ifdef CONFIG_GPSL5_SUPPORT
	/*static allocate chrdev */
	alloc_ret = register_chrdev_region(dev2, 1, GPS2_DRIVER_NAME);
	if (alloc_ret) {
		pr_info("fail to register chrdev\n");
		return alloc_ret;
	}

	cdev_init(&GPS2_cdev, &GPS2_fops);
	GPS2_cdev.owner = THIS_MODULE;

	cdev2_err = cdev_add(&GPS2_cdev, dev2, GPS2_devs);
	if (cdev2_err)
		goto error;
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE

	stpgps2_class = class_create(THIS_MODULE, "stpgps2");
	if (IS_ERR(stpgps2_class))
		goto error;
	stpgps2_dev = device_create(stpgps2_class, NULL, dev2, NULL, "stpgps2");
	if (IS_ERR(stpgps2_dev))
		goto error;
#endif
#endif
	pr_warn("%s driver(major %d) installed.\n", GPS_DRIVER_NAME, GPS_major);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 149)
	gps_wake_lock_ptr = wakeup_source_register(NULL, gps_wake_lock_name);
#else
	gps_wake_lock_ptr = wakeup_source_register(gps_wake_lock_name);
#endif
	if (!gps_wake_lock_ptr) {
		pr_info("%s %d: init wakeup source fail!", __func__, __LINE__);
		goto error;
	}

	sema_init(&status_mtx, 1);
	sema_init(&fwctl_mtx, 1);
	/* init_MUTEX(&wr_mtx); */
	sema_init(&wr_mtx, 1);
	/* init_MUTEX(&rd_mtx); */
	sema_init(&rd_mtx, 1);

#ifdef CONFIG_GPSL5_SUPPORT
	wakeup_source_init(&gps2_wake_lock, "gpswakelock");

	sema_init(&status_mtx2, 1);
	/* init_MUTEX(&wr_mtx); */
	sema_init(&wr_mtx2, 1);
	/* init_MUTEX(&rd_mtx); */
	sema_init(&rd_mtx2, 1);
#endif
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_linux_plat_drv_register();
#endif
	return 0;

error:

#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	if (!IS_ERR(stpgps_dev))
		device_destroy(stpgps_class, dev);
	if (!IS_ERR(stpgps_class)) {
		class_destroy(stpgps_class);
		stpgps_class = NULL;
	}
#ifdef CONFIG_GPSL5_SUPPORT
	if (!IS_ERR(stpgps2_dev))
		device_destroy(stpgps2_class, dev2);
	if (!IS_ERR(stpgps2_class)) {
		class_destroy(stpgps2_class);
		stpgps2_class = NULL;
	}
#endif
#endif
	if (cdev_err == 0)
		cdev_del(&GPS_cdev);
#ifdef CONFIG_GPSL5_SUPPORT
	if (cdev2_err == 0)
		cdev_del(&GPS2_cdev);
#endif
	if (alloc_ret == 0) {
		unregister_chrdev_region(dev, GPS_devs);
#ifdef CONFIG_GPSL5_SUPPORT
		unregister_chrdev_region(dev2, GPS2_devs);
#endif
	}

	return -1;
}

static void GPS_exit(void)
{
	dev_t dev = MKDEV(GPS_major, 0);
#ifdef CONFIG_GPSL5_SUPPORT
	dev_t dev2 = MKDEV(GPS_major, 1);
#endif
#if WMT_CREATE_NODE_DYNAMIC || REMOVE_MK_NODE
	device_destroy(stpgps_class, dev);
	class_destroy(stpgps_class);
	stpgps_class = NULL;
#ifdef CONFIG_GPSL5_SUPPORT
	device_destroy(stpgps2_class, dev2);
	class_destroy(stpgps2_class);
	stpgps2_class = NULL;
#endif
#endif

	cdev_del(&GPS_cdev);
	unregister_chrdev_region(dev, GPS_devs);
	pr_warn("%s driver removed.\n", GPS_DRIVER_NAME);

#ifdef CONFIG_GPSL5_SUPPORT
	cdev_del(&GPS2_cdev);
	unregister_chrdev_region(dev2, GPS2_devs);
	pr_info("%s driver removed.\n", GPS2_DRIVER_NAME);

#endif
#ifdef CONFIG_GPS_CTRL_LNA_SUPPORT
	gps_lna_linux_plat_drv_unregister();
#endif
	wakeup_source_unregister(gps_wake_lock_ptr);
}

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

/*****************************************************************************/
static int __init gps_mod_init(void)
{
	int ret = 0;

	mtk_wcn_stpgps_drv_init();
	#ifdef CONFIG_MTK_GPS_EMI
	mtk_gps_emi_init();
	#endif
	#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	mtk_gps_fw_log_init();
	#endif
	return ret;
}

/*****************************************************************************/
static void __exit gps_mod_exit(void)
{
	mtk_wcn_stpgps_drv_exit();
	#ifdef CONFIG_MTK_GPS_EMI
	mtk_gps_emi_exit();
	#endif
	#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	mtk_gps_fw_log_exit();
	#endif
}

module_init(gps_mod_init);
module_exit(gps_mod_exit);

int reference_count_bitmap[2] = {0};
void GPS_reference_count(enum gps_reference_count_cmd cmd, bool flag, int user)
{
	bool old_state = false;
	bool new_state = false;

	old_state = (reference_count_bitmap[0] & (0x01 << (int)cmd))
	|| (reference_count_bitmap[1] & (0x01 << FWLOG_CTRL_INNER));
	if (flag == true)
		reference_count_bitmap[user] |= (0x01 << (int)cmd);
	else
		reference_count_bitmap[user] &= ~(0x01 << (int)cmd);
	new_state = (reference_count_bitmap[0] & (0x01 << (int)cmd))
	|| (reference_count_bitmap[1] & (0x01 << FWLOG_CTRL_INNER));

	switch (cmd) {
	case FWLOG_CTRL_INNER:
#ifdef GPS_FWCTL_SUPPORT
		if (old_state != new_state)
			GPS_fwlog_ctrl_inner(new_state);
#endif
	break;
	case HANDLE_DESENSE:
		if (old_state != new_state)
			GPS_handle_desense(new_state);
	break;
	case FGGPS_FWCTL_EADY:
#ifdef GPS_FWCTL_SUPPORT
		if (old_state != new_state)
			fgGps_fwctl_ready = new_state;
#endif
	break;
	}
}

