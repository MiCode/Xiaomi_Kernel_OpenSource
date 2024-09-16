/*
* Copyright (C) 2016 MediaTek Inc.
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

#include "bt.h"
#include <linux/pm_wakeup.h>
#include <linux/version.h>
#include <linux/pm_qos.h>
#include <linux/notifier.h>
#include <linux/fb.h>

MODULE_LICENSE("Dual BSD/GPL");

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define BT_DRIVER_NAME "mtk_stp_bt_chrdev"
#define BT_DEV_MAJOR 192

#define VERSION "2.0"

#define COMBO_IOC_MAGIC             0xb0
#define COMBO_IOCTL_FW_ASSERT       _IOW(COMBO_IOC_MAGIC, 0, int)
#define COMBO_IOCTL_BT_SET_PSM      _IOW(COMBO_IOC_MAGIC, 1, bool)
#define COMBO_IOCTL_BT_IC_HW_VER    _IOR(COMBO_IOC_MAGIC, 2, void*)
#define COMBO_IOCTL_BT_IC_FW_VER    _IOR(COMBO_IOC_MAGIC, 3, void*)

#define BT_BUFFER_SIZE              2048
#define FTRACE_STR_LOG_SIZE         256
#define REG_READL(addr) readl((volatile uint32_t *)(addr))

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static INT32 BT_devs = 1;
static INT32 BT_major = BT_DEV_MAJOR;
module_param(BT_major, uint, 0);
static struct cdev BT_cdev;
#if CREATE_NODE_DYNAMIC
static struct class *stpbt_class;
static struct device *stpbt_dev;
#endif

static UINT8 i_buf[BT_BUFFER_SIZE]; /* Input buffer for read */
static UINT8 o_buf[BT_BUFFER_SIZE]; /* Output buffer for write */

static struct semaphore wr_mtx, rd_mtx;
static struct wakeup_source *bt_wakelock;
/* Wait queue for poll and read */
static wait_queue_head_t inq;
static DECLARE_WAIT_QUEUE_HEAD(BT_wq);
static INT32 flag;
static INT32 bt_ftrace_flag;
static bool btonflag = 0;
UINT32 gBtDbgLevel = BT_LOG_INFO;
struct bt_dbg_st g_bt_dbg_st;
#if (PM_QOS_CONTROL == 1)
static struct pm_qos_request qos_req;
static struct pm_qos_ctrl qos_ctrl;
#endif

/*
 * Reset flag for whole chip reset scenario, to indicate reset status:
 *   0 - normal, no whole chip reset occurs
 *   1 - reset start
 *   2 - reset end, have not sent Hardware Error event yet
 *   3 - reset end, already sent Hardware Error event
 */
static UINT32 rstflag;
static UINT8 HCI_EVT_HW_ERROR[] = {0x04, 0x10, 0x01, 0x00};
static loff_t rd_offset;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
extern int bt_dev_dbg_init(void);
extern int bt_dev_dbg_deinit(void);
extern int bt_dev_dbg_set_state(bool turn_on);

static INT32 ftrace_print(const PINT8 str, ...)
{
#ifdef CONFIG_TRACING
	va_list args;
	int ret = 0;
	INT8 temp_string[FTRACE_STR_LOG_SIZE];

	if (bt_ftrace_flag) {
		va_start(args, str);
		ret = vsnprintf(temp_string, FTRACE_STR_LOG_SIZE, str, args);
		va_end(args);
		if (ret < 0) {
			BT_LOG_PRT_ERR("error return value in vsnprintf ret = [%d]\n", ret);
			return 0;
		}
		trace_printk("%s\n", temp_string);
	}
#endif
	return 0;
}

static size_t bt_report_hw_error(char *buf, size_t count, loff_t *f_pos)
{
	size_t bytes_rest, bytes_read;

	if (*f_pos == 0)
		BT_LOG_PRT_INFO("Send Hardware Error event to stack to restart Bluetooth\n");

	bytes_rest = sizeof(HCI_EVT_HW_ERROR) - *f_pos;
	bytes_read = count < bytes_rest ? count : bytes_rest;
	memcpy(buf, HCI_EVT_HW_ERROR + *f_pos, bytes_read);
	*f_pos += bytes_read;

	return bytes_read;
}

static uint32_t inline bt_read_cr(unsigned char *cr_name, uint32_t addr)
{
	uint32_t value = 0;
	uint8_t *base = ioremap_nocache(addr, 0x10);

	if (base == NULL) {
		BT_LOG_PRT_WARN("remapping 0x%08x fail\n", addr);
	} else {
		value = REG_READL(base);
		iounmap(base);
		BT_LOG_PRT_INFO("%s[0x%08x], read[0x%08x]\n", cr_name, addr, value);
	}
	return value;
}

static struct notifier_block bt_fb_notifier;
static int bt_fb_notifier_callback(struct notifier_block
				*self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int32_t blank = 0;

	if ((event != FB_EVENT_BLANK))
		return 0;

	blank = *(int32_t *)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_POWERDOWN:
		if(btonflag == 1 && rstflag == 0) {
			BT_LOG_PRT_INFO("blank state [%ld]", blank);
			bt_read_cr("HOST_MAILBOX_BT_ADDR", 0x18007124);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int bt_fb_notify_register(void)
{
	int32_t ret;

	bt_fb_notifier.notifier_call = bt_fb_notifier_callback;

	ret = fb_register_client(&bt_fb_notifier);
	if (ret)
		BT_LOG_PRT_WARN("Register bt_fb_notifier failed:%d\n", ret);
	else
		BT_LOG_PRT_DBG("Register bt_fb_notifier succeed\n");

	return ret;
}

static void bt_fb_notify_unregister(void)
{
	fb_unregister_client(&bt_fb_notifier);
}

/*******************************************************************
*  WHOLE CHIP RESET message handler
********************************************************************
*/
static VOID bt_cdev_rst_cb(ENUM_WMTDRV_TYPE_T src,
			   ENUM_WMTDRV_TYPE_T dst, ENUM_WMTMSG_TYPE_T type, PVOID buf, UINT32 sz)
{
	ENUM_WMTRSTMSG_TYPE_T rst_msg;

	if (sz > sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
		BT_LOG_PRT_WARN("Invalid message format!\n");
		return;
	}

	memcpy((PINT8)&rst_msg, (PINT8)buf, sz);
	BT_LOG_PRT_DBG("src = %d, dst = %d, type = %d, buf = 0x%x sz = %d, max = %d\n",
		       src, dst, type, rst_msg, sz, WMTRSTMSG_RESET_MAX);
	if ((src == WMTDRV_TYPE_WMT) && (dst == WMTDRV_TYPE_BT) && (type == WMTMSG_TYPE_RESET)) {
		switch (rst_msg) {
		case WMTRSTMSG_RESET_START:
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
			bt_state_notify(OFF);
#endif
			BT_LOG_PRT_INFO("Whole chip reset start!\n");
			rstflag = 1;
			break;

		case WMTRSTMSG_RESET_END:
		case WMTRSTMSG_RESET_END_FAIL:
			if (rst_msg == WMTRSTMSG_RESET_END)
				BT_LOG_PRT_INFO("Whole chip reset end!\n");
			else
				BT_LOG_PRT_INFO("Whole chip reset fail!\n");
			rd_offset = 0;
			rstflag = 2;
			flag = 1;
			wake_up_interruptible(&inq);
			wake_up(&BT_wq);
			break;

		default:
			break;
		}
	}
}

static VOID BT_event_cb(VOID)
{
	BT_LOG_PRT_DBG("BT_event_cb\n");
	ftrace_print("%s get called", __func__);

	/*
	 * Hold wakelock for 100ms to avoid system enter suspend in such case:
	 *   FW has sent data to host, STP driver received the data and put it
	 *   into BT rx queue, then send sleep command and release wakelock as
	 *   quick sleep mechanism for low power, BT driver will wake up stack
	 *   hci thread stuck in poll or read.
	 *   But before hci thread comes to read data, system enter suspend,
	 *   hci command timeout timer keeps counting during suspend period till
	 *   expires, then the RTC interrupt wakes up system, command timeout
	 *   handler is executed and meanwhile the event is received.
	 *   This will false trigger FW assert and should never happen.
	 */
	__pm_wakeup_event(bt_wakelock, 100);

#if (PM_QOS_CONTROL == 1)
	/* pm qos control:
	 *   Set pm_qos to higher level for mass data transfer.
	 *   When rx packet reveived, schedule a work to restore pm_qos setting after 500ms.
	 *   If next packet is receiving before 500ms, this work will be cancel & re-schedule.
	 *   (500ms: better power performance after experiment)
	 */
	down(&qos_ctrl.sem);
	if(qos_ctrl.task != NULL ) {
		cancel_delayed_work(&qos_ctrl.work);
		if(qos_ctrl.is_hold == FALSE) {
			pm_qos_update_request(&qos_req, 1000);
			qos_ctrl.is_hold = TRUE;
			BT_LOG_PRT_INFO("[qos] is_hold[%d]\n", qos_ctrl.is_hold);
		}
		queue_delayed_work(qos_ctrl.task, &qos_ctrl.work, (500 * HZ) >> 10);
	}
	up(&qos_ctrl.sem);
#endif

	/*
	 * Finally, wake up any reader blocked in poll or read
	 */
	flag = 1;
	wake_up_interruptible(&inq);
	wake_up(&BT_wq);
	ftrace_print("%s wake_up triggered", __func__);
}

unsigned int BT_poll(struct file *filp, poll_table *wait)
{
	UINT32 mask = 0;

	if ((mtk_wcn_stp_is_rxqueue_empty(BT_TASK_INDX) && rstflag == 0) ||
	    (rstflag == 1) || (rstflag == 3)) {
		/*
		 * BT rx queue is empty, or whole chip reset start, or already sent Hardware Error event
		 * for whole chip reset end, add to wait queue.
		 */
		poll_wait(filp, &inq, wait);
		/*
		 * Check if condition changes before poll_wait return, in case of
		 * wake_up_interruptible is called before add_wait_queue, otherwise,
		 * do_poll will get into sleep and never be waken up until timeout.
		 */
		if (!((mtk_wcn_stp_is_rxqueue_empty(BT_TASK_INDX) && rstflag == 0) ||
		      (rstflag == 1) || (rstflag == 3)))
			mask |= POLLIN | POLLRDNORM;	/* Readable */
	} else {
		/* BT rx queue has valid data, or whole chip reset end, have not sent Hardware Error event yet */
		mask |= POLLIN | POLLRDNORM;	/* Readable */
	}

	/* Do we need condition here? */
	mask |= POLLOUT | POLLWRNORM;	/* Writable */
	ftrace_print("%s: return mask = 0x%04x", __func__, mask);

	return mask;
}

static ssize_t __bt_write(const PUINT8 buffer, size_t count)
{
	INT32 retval = 0;

	retval = mtk_wcn_stp_send_data(buffer, count, BT_TASK_INDX);

	if (retval < 0)
		BT_LOG_PRT_ERR("mtk_wcn_stp_send_data fail, retval %d\n", retval);
	else if (retval == 0) {
		/* Device cannot process data in time, STP queue is full and no space is available for write,
		 * native program should not call writev with no delay.
		 */
		BT_LOG_PRT_INFO_RATELIMITED("write count %zd, sent bytes %d, no space is available!\n", count, retval);
		retval = -EAGAIN;
	} else
		BT_LOG_PRT_DBG("write count %zd, sent bytes %d\n", count, retval);

	return retval;
}

ssize_t send_hci_frame(const PUINT8 buff, size_t count)
{
	ssize_t retval = 0;
	int retry = 0;

	down(&wr_mtx);

	do {
		if (retry > 0) {
			msleep(30);
			BT_LOG_PRT_ERR("Send hci cmd failed, retry %d time(s)\n", retry);
		}
		retval = __bt_write(buff, count);
		retry++;
	} while (retval == -EAGAIN && retry < 3);

	up(&wr_mtx);

	return retval;
}

ssize_t BT_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	INT32 retval = 0;
	size_t count = iov_iter_count(from);

	ftrace_print("%s get called, count %zu", __func__, count);
	down(&wr_mtx);

	BT_LOG_PRT_DBG("count %zd\n", count);

	if (rstflag) {
		BT_LOG_PRT_ERR("whole chip reset occurs! rstflag=%d\n", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BT_LOG_PRT_ERR("write count %zd exceeds max buffer size %d", count, BT_BUFFER_SIZE);
			retval = -EINVAL;
			goto OUT;
		}

		if (copy_from_iter(o_buf, count, from) != count) {
			retval = -EFAULT;
			goto OUT;
		}

		BT_LOG_PRT_DBG_RAW(o_buf, count, "%s: len[%d], TX: ", __func__, count);
		retval = __bt_write(o_buf, count);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

ssize_t BT_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 retval = 0;

	ftrace_print("%s get called, count %zu", __func__, count);
	down(&wr_mtx);

	BT_LOG_PRT_DBG("count %zd pos %lld\n", count, *f_pos);

	if (rstflag) {
		BT_LOG_PRT_ERR("whole chip reset occurs! rstflag=%d\n", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BT_LOG_PRT_ERR("write count %zd exceeds max buffer size %d", count, BT_BUFFER_SIZE);
			retval = -EINVAL;
			goto OUT;
		}

		if (copy_from_user(o_buf, buf, count)) {
			retval = -EFAULT;
			goto OUT;
		}

		BT_LOG_PRT_DBG_RAW(o_buf, count, "%s: len[%d], TX: ", __func__, count);
		retval = __bt_write(o_buf, count);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

ssize_t BT_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	INT32 retval = 0;

	ftrace_print("%s get called, count %zu", __func__, count);
	down(&rd_mtx);

	BT_LOG_PRT_DBG("count %zd pos %lld\n", count, *f_pos);

	if (rstflag) {
		while (rstflag != 2) {
			/*
			 * If nonblocking mode, return directly.
			 * O_NONBLOCK is specified during open()
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BT_LOG_PRT_ERR("Non-blocking read, whole chip reset occurs! rstflag=%d\n", rstflag);
				retval = -EIO;
				goto OUT;
			}

			wait_event(BT_wq, flag != 0);
			flag = 0;
		}
		/*
		 * Reset end, send Hardware Error event to stack only once.
		 * To avoid high frequency read from stack before process is killed, set rstflag to 3
		 * to block poll and read after Hardware Error event is sent.
		 */
		retval = bt_report_hw_error(i_buf, count, &rd_offset);
		if (rd_offset == sizeof(HCI_EVT_HW_ERROR)) {
			rd_offset = 0;
			rstflag = 3;
		}

		if (copy_to_user(buf, i_buf, retval)) {
			retval = -EFAULT;
			if (rstflag == 3)
				rstflag = 2;
		}

		goto OUT;
	}

	if (count > BT_BUFFER_SIZE) {
		count = BT_BUFFER_SIZE;
		BT_LOG_PRT_WARN("Shorten read count from %zd to %d\n", count, BT_BUFFER_SIZE);
	}

	do {
		retval = mtk_wcn_stp_receive_data(i_buf, count, BT_TASK_INDX);
		if (retval < 0) {
			BT_LOG_PRT_ERR("mtk_wcn_stp_receive_data fail, retval %d\n", retval);
			goto OUT;
		} else if (retval == 0) {	/* Got nothing, wait for STP's signal */
			/*
			 * If nonblocking mode, return directly.
			 * O_NONBLOCK is specified during open()
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BT_LOG_PRT_ERR("Non-blocking read, no data is available!\n");
				retval = -EAGAIN;
				goto OUT;
			}

			wait_event(BT_wq, flag != 0);
			flag = 0;
		} else {	/* Got something from STP driver */
			// for bt_dbg user trx function
			if (g_bt_dbg_st.trx_enable) {
				g_bt_dbg_st.trx_cb(i_buf, retval);
			}
			//BT_LOG_PRT_DBG("Read bytes %d\n", retval);
			BT_LOG_PRT_DBG_RAW(i_buf, retval, "%s: len[%d], RX: ", __func__, retval);
			break;
		}
	} while (!mtk_wcn_stp_is_rxqueue_empty(BT_TASK_INDX) && rstflag == 0);

	if (retval == 0) {
		if (rstflag != 2) {	/* Should never happen */
			WARN(1, "Blocking read is waken up with no data but rstflag=%d\n", rstflag);
			retval = -EIO;
			goto OUT;
		} else {	/* Reset end, send Hardware Error event only once */
			retval = bt_report_hw_error(i_buf, count, &rd_offset);
			if (rd_offset == sizeof(HCI_EVT_HW_ERROR)) {
				rd_offset = 0;
				rstflag = 3;
			}
		}
	}

	if (copy_to_user(buf, i_buf, retval)) {
		retval = -EFAULT;
		if (rstflag == 3)
			rstflag = 2;
	}

OUT:
	up(&rd_mtx);
	return retval;
}

/* int BT_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) */
long BT_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	INT32 retval = 0;
	UINT32 reason;
	UINT32 ver = 0;

	BT_LOG_PRT_DBG("cmd: 0x%08x\n", cmd);

	switch (cmd) {
	case COMBO_IOCTL_FW_ASSERT:
		/* Trigger FW assert for debug */
		reason = (UINT32)arg & 0xFFFF;
		BT_LOG_PRT_INFO("Host trigger FW assert......, reason:%d\n", reason);
		if (reason == 31) /* HCI command timeout */
			BT_LOG_PRT_INFO("HCI command timeout OpCode 0x%04x\n", ((UINT32)arg >> 16) & 0xFFFF);

		if (mtk_wcn_wmt_assert(WMTDRV_TYPE_BT, reason) == MTK_WCN_BOOL_TRUE) {
			BT_LOG_PRT_INFO("Host trigger FW assert succeed\n");
			retval = 0;
		} else {
			BT_LOG_PRT_ERR("Host trigger FW assert failed\n");
			retval = -EBUSY;
		}
		break;
	case COMBO_IOCTL_BT_SET_PSM:
		/* BT stack may need to dynamically enable/disable Power Saving Mode
		 * in some scenarios for performance, e.g. A2DP chopping.
		 */
		BT_LOG_PRT_INFO("BT stack change PSM setting: %lu\n", arg);
		retval = mtk_wcn_wmt_psm_ctrl((MTK_WCN_BOOL)arg);
		break;
	case COMBO_IOCTL_BT_IC_HW_VER:
		ver = mtk_wcn_wmt_ic_info_get(WMTCHIN_HWVER);
		BT_LOG_PRT_INFO("HW ver: 0x%x\n", ver);
		if (copy_to_user((UINT32 __user *)arg, &ver, sizeof(ver)))
			retval = -EFAULT;
		break;
	case COMBO_IOCTL_BT_IC_FW_VER:
		ver = mtk_wcn_wmt_ic_info_get(WMTCHIN_FWVER);
		BT_LOG_PRT_INFO("FW ver: 0x%x\n", ver);
		if (copy_to_user((UINT32 __user *)arg, &ver, sizeof(ver)))
			retval = -EFAULT;
		break;
	default:
		BT_LOG_PRT_ERR("Unknown cmd: 0x%08x\n", cmd);
		retval = -EOPNOTSUPP;
		break;
	}

	return retval;
}

long BT_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return BT_unlocked_ioctl(filp, cmd, arg);
}

#if (PM_QOS_CONTROL == 1)
static void pm_qos_release(struct work_struct *pwork)
{
	pm_qos_update_request(&qos_req, PM_QOS_DEFAULT_VALUE);
	qos_ctrl.is_hold = FALSE;
	BT_LOG_PRT_INFO("[qos] is_hold[%d]\n", qos_ctrl.is_hold);
}
#endif

static int BT_open(struct inode *inode, struct file *file)
{
	if(btonflag) {
		BT_LOG_PRT_WARN("BT already on!\n");
		return -EIO;
	}
	BT_LOG_PRT_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	/* Turn on BT */
	if (mtk_wcn_wmt_func_on(WMTDRV_TYPE_BT) == MTK_WCN_BOOL_FALSE) {
		BT_LOG_PRT_WARN("WMT turn on BT fail!\n");
		return -EIO;
	}

	BT_LOG_PRT_INFO("WMT turn on BT OK!\n");

	if (mtk_wcn_stp_is_ready() == MTK_WCN_BOOL_FALSE) {

		BT_LOG_PRT_ERR("STP is not ready!\n");
		mtk_wcn_wmt_func_off(WMTDRV_TYPE_BT);
		return -EIO;
	}

	mtk_wcn_stp_set_bluez(0);

	BT_LOG_PRT_INFO("Now it's in MTK Bluetooth Mode\n");
	BT_LOG_PRT_INFO("STP is ready!\n");

	BT_LOG_PRT_DBG("Register BT event callback!\n");
	mtk_wcn_stp_register_event_cb(BT_TASK_INDX, BT_event_cb);

	BT_LOG_PRT_DBG("Register BT reset callback!\n");
	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_BT, bt_cdev_rst_cb);

	rstflag = 0;
	bt_ftrace_flag = 1;
	btonflag = 1;

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	bt_state_notify(ON);
#endif
	bt_dev_dbg_set_state(TRUE);

#if (PM_QOS_CONTROL == 1)
	down(&qos_ctrl.sem);
	pm_qos_update_request(&qos_req, PM_QOS_DEFAULT_VALUE);
	qos_ctrl.is_hold = FALSE;
	qos_ctrl.task = create_singlethread_workqueue("pm_qos_task");
	if (!qos_ctrl.task){
		BT_LOG_PRT_ERR("fail to create pm_qos_task");
		return -EIO;
	}
	INIT_DELAYED_WORK(&qos_ctrl.work, pm_qos_release);
	up(&qos_ctrl.sem);
#endif
	bt_fb_notify_register();

	return 0;
}

static int BT_close(struct inode *inode, struct file *file)
{
	BT_LOG_PRT_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);

	bt_fb_notify_unregister();
	bt_dev_dbg_set_state(FALSE);
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	bt_state_notify(OFF);
#endif

	rstflag = 0;
	bt_ftrace_flag = 0;
	btonflag = 0;

	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_BT);
	mtk_wcn_stp_register_event_cb(BT_TASK_INDX, NULL);

#if (PM_QOS_CONTROL == 1)
	down(&qos_ctrl.sem);
	if(qos_ctrl.task != NULL) {
		BT_LOG_PRT_INFO("[qos] cancel delayed work\n");
		cancel_delayed_work(&qos_ctrl.work);
		flush_workqueue(qos_ctrl.task);
		destroy_workqueue(qos_ctrl.task);
		qos_ctrl.task = NULL;
	}
	pm_qos_update_request(&qos_req, PM_QOS_DEFAULT_VALUE);
	qos_ctrl.is_hold = FALSE;
	up(&qos_ctrl.sem);
#endif

	if (mtk_wcn_wmt_func_off(WMTDRV_TYPE_BT) == MTK_WCN_BOOL_FALSE) {
		BT_LOG_PRT_ERR("WMT turn off BT fail!\n");
		return -EIO;	/* Mostly, native program will not check this return value. */
	}

	BT_LOG_PRT_INFO("WMT turn off BT OK!\n");
	return 0;
}

const struct file_operations BT_fops = {
	.open = BT_open,
	.release = BT_close,
	.read = BT_read,
	.write = BT_write,
	.write_iter = BT_write_iter,
	/* .ioctl = BT_ioctl, */
	.unlocked_ioctl = BT_unlocked_ioctl,
	.compat_ioctl = BT_compat_ioctl,
	.poll = BT_poll
};

static int BT_init(void)
{
	dev_t dev;
	INT32 alloc_ret = 0;
	INT32 cdv_err = 0;
	dev = MKDEV(BT_major, 0);

	/* Initialize wait queue */
	init_waitqueue_head(&(inq));
	/* Initialize wake lock */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 149)
	BT_LOG_PRT_INFO("wakeup_source_register() with kernel-4.14.149\n");
	bt_wakelock = wakeup_source_register(NULL, "bt_drv");
#else
	bt_wakelock = wakeup_source_register("bt_drv");
#endif
	if(!bt_wakelock) {
		BT_LOG_PRT_ERR("%s: init bt_wakelock failed!\n", __func__);
	}

	/* Allocate char device */
	alloc_ret = register_chrdev_region(dev, BT_devs, BT_DRIVER_NAME);
	if (alloc_ret) {
		BT_LOG_PRT_ERR("Failed to register device numbers\n");
		return alloc_ret;
	}

	cdev_init(&BT_cdev, &BT_fops);
	BT_cdev.owner = THIS_MODULE;

	cdv_err = cdev_add(&BT_cdev, dev, BT_devs);
	if (cdv_err)
		goto error;

#if CREATE_NODE_DYNAMIC /* mknod replace */
	stpbt_class = class_create(THIS_MODULE, "stpbt");
	if (IS_ERR(stpbt_class))
		goto error;
	stpbt_dev = device_create(stpbt_class, NULL, dev, NULL, "stpbt");
	if (IS_ERR(stpbt_dev))
		goto error;
#endif

	BT_LOG_PRT_INFO("%s driver(major %d) installed\n", BT_DRIVER_NAME, BT_major);

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	fw_log_bt_init();
#endif
	bt_dev_dbg_init();

#if (PM_QOS_CONTROL == 1)
	pm_qos_add_request(&qos_req, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	sema_init(&qos_ctrl.sem, 1);
#endif

	return 0;

error:
#if CREATE_NODE_DYNAMIC
	if (stpbt_dev && !IS_ERR(stpbt_dev)) {
		device_destroy(stpbt_class, dev);
		stpbt_dev = NULL;
	}
	if (stpbt_class && !IS_ERR(stpbt_class)) {
		class_destroy(stpbt_class);
		stpbt_class = NULL;
	}
#endif
	if (cdv_err == 0)
		cdev_del(&BT_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(dev, BT_devs);

	return -1;
}

static void BT_exit(void)
{
	dev_t dev;

#if (PM_QOS_CONTROL == 1)
	pm_qos_remove_request(&qos_req);
#endif

	bt_dev_dbg_deinit();
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
	fw_log_bt_exit();
#endif

	dev = MKDEV(BT_major, 0);
	/* Destroy wake lock*/
	wakeup_source_unregister(bt_wakelock);

#if CREATE_NODE_DYNAMIC
	if (stpbt_dev && !IS_ERR(stpbt_dev)) {
		device_destroy(stpbt_class, dev);
		stpbt_dev = NULL;
	}
	if (stpbt_class && !IS_ERR(stpbt_class)) {
		class_destroy(stpbt_class);
		stpbt_class = NULL;
	}
#endif

	cdev_del(&BT_cdev);
	unregister_chrdev_region(dev, BT_devs);

	BT_LOG_PRT_INFO("%s driver removed\n", BT_DRIVER_NAME);
}

#ifdef MTK_WCN_REMOVE_KERNEL_MODULE

int mtk_wcn_stpbt_drv_init(void)
{
	return BT_init();
}
EXPORT_SYMBOL(mtk_wcn_stpbt_drv_init);

void mtk_wcn_stpbt_drv_exit(void)
{
	return BT_exit();
}
EXPORT_SYMBOL(mtk_wcn_stpbt_drv_exit);

#else

module_init(BT_init);
module_exit(BT_exit);

#endif
