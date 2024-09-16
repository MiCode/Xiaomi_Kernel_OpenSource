// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "btmtk_main.h"

MODULE_LICENSE("Dual BSD/GPL");

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define BT_DRIVER_NAME      "mtk_bt_chrdev"
#define BT_DRIVER_NODE_NAME "stpbt"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define BT_BUFFER_SIZE                (2048)
#define FTRACE_STR_LOG_SIZE           (256)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum chip_reset_state {
	CHIP_RESET_NONE,
	CHIP_RESET_START,
	CHIP_RESET_END,
	CHIP_RESET_NOTIFIED
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static int32_t BT_devs = 1;
static int32_t BT_major = 192;
module_param(BT_major, uint, 0);
static struct cdev BT_cdev;
#if CREATE_NODE_DYNAMIC
static struct class *BT_class;
static struct device *BT_dev;
#endif

static uint8_t i_buf[BT_BUFFER_SIZE]; /* Input buffer for read */
static uint8_t o_buf[BT_BUFFER_SIZE]; /* Output buffer for write */

extern struct btmtk_dev *g_bdev;
static struct semaphore wr_mtx, rd_mtx;
static struct bt_wake_lock bt_wakelock;
/* Wait queue for poll and read */
static wait_queue_head_t inq;
static DECLARE_WAIT_QUEUE_HEAD(BT_wq);
static int32_t flag;
static int32_t bt_ftrace_flag;
/*
 * Reset flag for whole chip reset scenario, to indicate reset status:
 *   0 - normal, no whole chip reset occurs
 *   1 - reset start
 *   2 - reset end, have not sent Hardware Error event yet
 *   3 - reset end, already sent Hardware Error event
 */
static uint32_t rstflag = CHIP_RESET_NONE;
static uint8_t HCI_EVT_HW_ERROR[] = {0x04, 0x10, 0x01, 0x00};
static loff_t rd_offset;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
extern int bt_dev_dbg_init(void);
extern int bt_dev_dbg_deinit(void);
extern int bt_dev_dbg_set_state(bool turn_on);

static int32_t ftrace_print(const uint8_t *str, ...)
{
#ifdef CONFIG_TRACING
	va_list args;
	uint8_t temp_string[FTRACE_STR_LOG_SIZE];

	if (bt_ftrace_flag) {
		va_start(args, str);
		if (vsnprintf(temp_string, FTRACE_STR_LOG_SIZE, str, args) < 0)
			BTMTK_INFO("%s: vsnprintf error", __func__);
		va_end(args);
		trace_printk("%s\n", temp_string);
	}
#endif
	return 0;
}

static size_t bt_report_hw_error(uint8_t *buf, size_t count, loff_t *f_pos)
{
	size_t bytes_rest, bytes_read;

	if (*f_pos == 0)
		BTMTK_INFO("Send Hardware Error event to stack to restart Bluetooth");

	bytes_rest = sizeof(HCI_EVT_HW_ERROR) - *f_pos;
	bytes_read = count < bytes_rest ? count : bytes_rest;
	memcpy(buf, HCI_EVT_HW_ERROR + *f_pos, bytes_read);
	*f_pos += bytes_read;

	return bytes_read;
}

static void bt_state_cb(enum bt_state state)
{
	switch (state) {

	case FUNC_ON:
		rstflag = CHIP_RESET_NONE;
		break;
	case RESET_START:
		rstflag = CHIP_RESET_START;
		break;
	case FUNC_OFF:
		if (rstflag != CHIP_RESET_START) {
			rstflag = CHIP_RESET_NONE;
			break;
		}
	case RESET_END:
		rstflag = CHIP_RESET_END;
		rd_offset = 0;
		flag = 1;
		wake_up_interruptible(&inq);
		wake_up(&BT_wq);
		break;
	default:
		break;
	}
	return;
}

#if 1
static void BT_event_cb(void)
{
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
	bt_hold_wake_lock_timeout(&bt_wakelock, 100);

	/*
	 * Finally, wake up any reader blocked in poll or read
	 */
	flag = 1;
	wake_up_interruptible(&inq);
	wake_up(&BT_wq);
	ftrace_print("%s wake_up triggered", __func__);
}
#endif

static unsigned int BT_poll(struct file *filp, poll_table *wait)
{
	uint32_t mask = 0;

	if ((!btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE) ||
	    (rstflag == CHIP_RESET_START) || (rstflag == CHIP_RESET_NOTIFIED)) {
		/*
		 * BT RX queue is empty, or whole chip reset start, or already sent Hardware Error event
		 * for whole chip reset end, add to wait queue.
		 */
		poll_wait(filp, &inq, wait);
		/*
		 * Check if condition changes before poll_wait return, in case of
		 * wake_up_interruptible is called before add_wait_queue, otherwise,
		 * do_poll will get into sleep and never be waken up until timeout.
		 */
		if (!((!btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE) ||
		      (rstflag == CHIP_RESET_START) || (rstflag == CHIP_RESET_NOTIFIED)))
			mask |= POLLIN | POLLRDNORM;	/* Readable */
	} else {
		/* BT RX queue has valid data, or whole chip reset end, have not sent Hardware Error event yet */
		mask |= POLLIN | POLLRDNORM;	/* Readable */
	}

	/* Do we need condition here? */
	mask |= POLLOUT | POLLWRNORM;	/* Writable */
	ftrace_print("%s: return mask = 0x%04x", __func__, mask);

	return mask;
}

static ssize_t __bt_write(uint8_t *buf, size_t count, uint32_t flags)
{
	int32_t retval = 0;

	retval = btmtk_send_data(g_bdev->hdev, buf, count);

	if (retval < 0)
		BTMTK_ERR("bt_core_send_data failed, retval %d", retval);
	else if (retval == 0) {
		/*
		  * TX queue cannot be digested in time and no space is available for write.
		  *
		  * If nonblocking mode, return -EAGAIN to let user retry,
		  * native program should not call write with no delay.
		  */
		if (flags & O_NONBLOCK) {
			BTMTK_WARN("Non-blocking write, no space is available!");
			retval = -EAGAIN;
		} else {
			/*TODO: blocking write case */
		}
	} else
		BTMTK_DBG("Write bytes %d/%zd", retval, count);

	return retval;
}

static ssize_t BT_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	ssize_t retval = 0;
	size_t count = iov_iter_count(from);

	ftrace_print("%s get called, count %zd", __func__, count);
	down(&wr_mtx);

	BTMTK_DBG("count %zd", count);

	if (rstflag != CHIP_RESET_NONE) {
		BTMTK_ERR("whole chip reset occurs! rstflag=%d", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BTMTK_WARN("Shorten write count from %zd to %d", count, BT_BUFFER_SIZE);
			count = BT_BUFFER_SIZE;
		}

		if (copy_from_iter(o_buf, count, from) != count) {
			retval = -EFAULT;
			goto OUT;
		}

		retval = __bt_write(o_buf, count, iocb->ki_filp->f_flags);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

static ssize_t BT_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;

	ftrace_print("%s get called, count %zd", __func__, count);
	down(&wr_mtx);

	BTMTK_DBG("count %zd pos %lld", count, *f_pos);

	if (rstflag != CHIP_RESET_NONE) {
		BTMTK_ERR("whole chip reset occurs! rstflag=%d", rstflag);
		retval = -EIO;
		goto OUT;
	}

	if (count > 0) {
		if (count > BT_BUFFER_SIZE) {
			BTMTK_WARN("Shorten write count from %zd to %d", count, BT_BUFFER_SIZE);
			count = BT_BUFFER_SIZE;
		}

		if (copy_from_user(o_buf, buf, count)) {
			retval = -EFAULT;
			goto OUT;
		}

		retval = __bt_write(o_buf, count, filp->f_flags);
	}

OUT:
	up(&wr_mtx);
	return retval;
}

static ssize_t BT_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;

	ftrace_print("%s get called, count %zd", __func__, count);
	down(&rd_mtx);

	BTMTK_DBG("%s: count %zd pos %lld", __func__, count, *f_pos);

	if (rstflag != CHIP_RESET_NONE) {
		while (rstflag != CHIP_RESET_END && rstflag != CHIP_RESET_NONE) {
			/*
			 * If nonblocking mode, return -EIO directly.
			 * O_NONBLOCK is specified during open().
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BTMTK_ERR("Non-blocking read, whole chip reset occurs! rstflag=%d", rstflag);
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
			rstflag = CHIP_RESET_NOTIFIED;
		}

		if (copy_to_user(buf, i_buf, retval)) {
			retval = -EFAULT;
			if (rstflag == CHIP_RESET_NOTIFIED)
				rstflag = CHIP_RESET_END;
		}

		goto OUT;
	}

	if (count > BT_BUFFER_SIZE) {
		BTMTK_WARN("Shorten read count from %zd to %d", count, BT_BUFFER_SIZE);
		count = BT_BUFFER_SIZE;
	}

	do {
		retval = btmtk_receive_data(g_bdev->hdev, i_buf, count);
		if (retval < 0) {
			BTMTK_ERR("bt_core_receive_data failed, retval %d", retval);
			goto OUT;
		} else if (retval == 0) { /* Got nothing, wait for RX queue's signal */
			/*
			 * If nonblocking mode, return -EAGAIN to let user retry.
			 * O_NONBLOCK is specified during open().
			 */
			if (filp->f_flags & O_NONBLOCK) {
				BTMTK_ERR("Non-blocking read, no data is available!");
				retval = -EAGAIN;
				goto OUT;
			}

			wait_event(BT_wq, flag != 0);
			flag = 0;
		} else { /* Got something from RX queue */
			break;
		}
	} while (btmtk_rx_data_valid() && rstflag == CHIP_RESET_NONE);

	if (retval == 0) {
		if (rstflag != CHIP_RESET_END) { /* Should never happen */
			WARN(1, "Blocking read is waken up in unexpected case, rstflag=%d", rstflag);
			retval = -EIO;
			goto OUT;
		} else { /* Reset end, send Hardware Error event only once */
			retval = bt_report_hw_error(i_buf, count, &rd_offset);
			if (rd_offset == sizeof(HCI_EVT_HW_ERROR)) {
				rd_offset = 0;
				rstflag = CHIP_RESET_NOTIFIED;
			}
		}
	}

	if (copy_to_user(buf, i_buf, retval)) {
		retval = -EFAULT;
		if (rstflag == CHIP_RESET_NOTIFIED)
			rstflag = CHIP_RESET_END;
	}

OUT:
	up(&rd_mtx);
	return retval;
}

static long BT_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int32_t retval = 0;

	BTMTK_DBG("cmd: 0x%08x", cmd);
	return retval;
}

static long BT_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return BT_unlocked_ioctl(filp, cmd, arg);
}

static int BT_open(struct inode *inode, struct file *file)
{
	int32_t ret;

	bt_hold_wake_lock(&bt_wakelock);
	BTMTK_INFO("major %d minor %d (pid %d)", imajor(inode), iminor(inode), current->pid);

	/* Turn on BT */
	ret = g_bdev->hdev->open(g_bdev->hdev);
	if (ret) {
		BTMTK_ERR("BT turn on fail!");
		bt_release_wake_lock(&bt_wakelock);
		return ret;
	}

	BTMTK_INFO("BT turn on OK!");

	btmtk_register_rx_event_cb(g_bdev, BT_event_cb);

	bt_ftrace_flag = 1;
	bt_dev_dbg_set_state(TRUE);
	bt_release_wake_lock(&bt_wakelock);
	return 0;
}

static int BT_close(struct inode *inode, struct file *file)
{
	int32_t ret;

	bt_hold_wake_lock(&bt_wakelock);
	BTMTK_INFO("major %d minor %d (pid %d)", imajor(inode), iminor(inode), current->pid);
	bt_dev_dbg_set_state(FALSE);
	bt_ftrace_flag = 0;
	//bt_core_unregister_rx_event_cb();

	ret = g_bdev->hdev->close(g_bdev->hdev);
	if (ret) {
		BTMTK_ERR("BT turn off fail!");
		bt_release_wake_lock(&bt_wakelock);
		return ret;
	}

	BTMTK_INFO("BT turn off OK!");

	bt_release_wake_lock(&bt_wakelock);
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
	int32_t alloc_err = 0;
	int32_t cdv_err = 0;
	dev_t dev = MKDEV(BT_major, 0);

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);
	init_waitqueue_head(&inq);

	/* Initialize wake lock for I/O operation */
	strncpy(bt_wakelock.name, "bt_drv_io", 9);
	bt_wakelock.name[9] = 0;
	bt_wake_lock_init(&bt_wakelock);

	main_driver_init();

	g_bdev->state_change_cb[1] = bt_state_cb;
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#if 0
	fw_log_bt_init();
#endif
#endif

	/* Allocate char device */
	alloc_err = register_chrdev_region(dev, BT_devs, BT_DRIVER_NAME);
	if (alloc_err) {
		BTMTK_ERR("Failed to register device numbers");
		goto alloc_error;
	}

	cdev_init(&BT_cdev, &BT_fops);
	BT_cdev.owner = THIS_MODULE;

	cdv_err = cdev_add(&BT_cdev, dev, BT_devs);
	if (cdv_err)
		goto cdv_error;

#if CREATE_NODE_DYNAMIC /* mknod replace */
	BT_class = class_create(THIS_MODULE, BT_DRIVER_NODE_NAME);
	if (IS_ERR(BT_class))
		goto create_node_error;
	BT_dev = device_create(BT_class, NULL, dev, NULL, BT_DRIVER_NODE_NAME);
	if (IS_ERR(BT_dev))
		goto create_node_error;
#endif

	bt_dev_dbg_init();
	BTMTK_INFO("%s driver(major %d) installed", BT_DRIVER_NAME, BT_major);
	return 0;

#if CREATE_NODE_DYNAMIC
create_node_error:
	if (BT_class && !IS_ERR(BT_class)) {
		class_destroy(BT_class);
		BT_class = NULL;
	}

	cdev_del(&BT_cdev);
#endif

cdv_error:
	unregister_chrdev_region(dev, BT_devs);

alloc_error:
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#if 0
	fw_log_bt_exit();
#endif
#endif
	main_driver_exit();
	return -1;
}

static void BT_exit(void)
{
	dev_t dev = MKDEV(BT_major, 0);
	bt_dev_dbg_deinit();

#if CREATE_NODE_DYNAMIC
	if (BT_dev && !IS_ERR(BT_dev)) {
		device_destroy(BT_class, dev);
		BT_dev = NULL;
	}
	if (BT_class && !IS_ERR(BT_class)) {
		class_destroy(BT_class);
		BT_class = NULL;
	}
#endif

	cdev_del(&BT_cdev);
	unregister_chrdev_region(dev, BT_devs);

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#if 0
	fw_log_bt_exit();
#endif
#endif
	g_bdev->state_change_cb[1] = NULL;
	main_driver_exit();

	/* Destroy wake lock */
	bt_wake_lock_deinit(&bt_wakelock);

	BTMTK_INFO("%s driver removed", BT_DRIVER_NAME);
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
