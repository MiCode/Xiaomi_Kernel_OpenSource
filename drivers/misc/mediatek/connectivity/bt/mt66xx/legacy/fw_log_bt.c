/*
*  Copyright (C) 2016 MediaTek Inc.
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

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#include "bt.h"
#include "connsys_debug_utility.h"

MODULE_LICENSE("Dual BSD/GPL");

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define BT_LOG_NODE_NAME "fw_log_bt"

#define BT_FW_LOG_IOC_MAGIC          (0xfc)
#define BT_FW_LOG_IOCTL_ON_OFF      _IOW(BT_FW_LOG_IOC_MAGIC, 0, int)
#define BT_FW_LOG_IOCTL_SET_LEVEL   _IOW(BT_FW_LOG_IOC_MAGIC, 1, int)
#define BT_FW_LOG_IOCTL_GET_LEVEL   _IOW(BT_FW_LOG_IOC_MAGIC, 2, int)

static unsigned char g_bt_on = OFF;
static unsigned char g_log_on = OFF;
static unsigned char g_log_level = DEFAULT_LEVEL;
static unsigned char g_log_current = OFF;

#define BT_LOG_BUFFER_SIZE  512

static struct cdev log_cdev;
#if CREATE_NODE_DYNAMIC
static struct class *log_class;
static struct device *log_dev;
#endif
static dev_t devno;

wait_queue_head_t BT_log_wq;

static struct semaphore ioctl_mtx;

static int ascii_to_hex(unsigned char ascii, unsigned char *hex)
{
	int ret = 0;

	if('0' <= ascii && ascii <= '9')
		*hex = ascii - '0';
	else if ('a' <= ascii && ascii <= 'f')
		*hex = ascii - 'a' + 10;
	else if ('A' <= ascii && ascii <= 'F')
		*hex = ascii - 'A' + 10;
	else
		ret = -1;

	return ret;
}

static int set_fw_log(unsigned char flag)
{
	ssize_t retval = 0;

	/* Opcode 0xfc5d TCI_MTK_DEBUG_VERSION_INFO */
	unsigned char HCI_CMD_FW_LOG_DEBUG[] = {0x01, 0x5d, 0xfc, 0x04, 0x02, 0x00, 0x01, 0xff}; // Via EMI

	HCI_CMD_FW_LOG_DEBUG[7] = flag;
	BT_LOG_PRT_INFO("hci_cmd: %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
			HCI_CMD_FW_LOG_DEBUG[0], HCI_CMD_FW_LOG_DEBUG[1],
			HCI_CMD_FW_LOG_DEBUG[2], HCI_CMD_FW_LOG_DEBUG[3],
			HCI_CMD_FW_LOG_DEBUG[4], HCI_CMD_FW_LOG_DEBUG[5],
			HCI_CMD_FW_LOG_DEBUG[6], HCI_CMD_FW_LOG_DEBUG[7]);

	retval = send_hci_frame(HCI_CMD_FW_LOG_DEBUG, sizeof(HCI_CMD_FW_LOG_DEBUG));

	if (likely(retval == sizeof(HCI_CMD_FW_LOG_DEBUG)))
		return 0;
	else if (retval < 0)
		return retval;
	else {
		BT_LOG_PRT_ERR("Only partial sent %zu bytes, but hci cmd has %zu bytes", retval, sizeof(HCI_CMD_FW_LOG_DEBUG));
		return -EFAULT;
	}
}

void bt_state_notify(UINT32 on_off)
{
	BT_LOG_PRT_INFO("g_bt_on %d, on_off %d\n", g_bt_on, on_off);

	if (g_bt_on == on_off) {
		// no change.
	} else {
		// changed.
		if (on_off == OFF) { // should turn off.
			g_bt_on = OFF;
			BT_LOG_PRT_INFO("BT func off, no need to send hci cmd\n");
		} else {
			g_bt_on = ON;
			if(g_log_current)
				set_fw_log(g_log_current);
		}
	}
}

long fw_log_bt_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	unsigned char log_tmp = OFF;

	down(&ioctl_mtx);
	switch (cmd) {
		case BT_FW_LOG_IOCTL_ON_OFF:
			/* connsyslogger daemon dynamically enable/disable Picus log */
			BT_LOG_PRT_INFO("BT_FW_LOG_IOCTL_ON_OFF: arg(%lu), g_bt_on(0x%02x), g_log_on(0x%02x), g_log_level(0x%02x), g_log_current(0x%02x)\n",
					arg, g_bt_on, g_log_on, g_log_level, g_log_current);
			log_tmp = (arg == 0 ? OFF: ON);
			if (log_tmp == g_log_on) // no change
				break;
			else { // changed
				g_log_on = log_tmp;
				g_log_current = g_log_on & g_log_level;
				if (g_bt_on)
					retval = set_fw_log(g_log_current);
			}
			break;
		case BT_FW_LOG_IOCTL_SET_LEVEL:
			/* connsyslogger daemon dynamically set Picus log level */
			BT_LOG_PRT_INFO("BT_FW_LOG_IOCTL_SET_LEVEL: arg(%lu), g_bt_on(0x%02x),  g_log_on(0x%02x), g_log_level(0x%02x), g_log_current(0x%02x)\n",
					arg,  g_bt_on, g_log_on, g_log_level, g_log_current);
			log_tmp = (unsigned char)arg;
			if(log_tmp == g_log_level) // no change
				break;
			else {
				g_log_level = log_tmp;
				g_log_current = g_log_on & g_log_level;
				if (g_bt_on & g_log_on) // driver on and log on
					retval = set_fw_log(g_log_current);
			}
			break;
		case BT_FW_LOG_IOCTL_GET_LEVEL:
			retval = g_log_level;
			BT_LOG_PRT_INFO("BT_FW_LOG_IOCTL_GET_LEVEL: %ld\n", retval);
			break;
		default:
			BT_LOG_PRT_ERR("Unknown cmd: 0x%08x\n", cmd);
			retval = -EOPNOTSUPP;
			break;
	}

	up(&ioctl_mtx);
	return retval;
}

long fw_log_bt_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return fw_log_bt_unlocked_ioctl(filp, cmd, arg);
}

static void fw_log_bt_event_cb(void)
{
	BT_LOG_PRT_DBG("fw_log_bt_event_cb");
	wake_up_interruptible(&BT_log_wq);
}

static unsigned int fw_log_bt_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &BT_log_wq, wait);
	if (connsys_log_get_buf_size(CONNLOG_TYPE_BT) > 0) {
		mask = (POLLIN | POLLRDNORM);
	}
	return mask;
}

static ssize_t fw_log_bt_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	UINT8 tmp_buf[BT_LOG_BUFFER_SIZE] = {0};
	UINT8 hci_cmd[BT_LOG_BUFFER_SIZE] = {0};
	UINT8 tmp = 0, tmp_h = 0;
	size_t i = 0, j = 0, k = 0;

	if(count >= BT_LOG_BUFFER_SIZE) {
		BT_LOG_PRT_ERR("write count %zd exceeds max buffer size %d", count, BT_LOG_BUFFER_SIZE);
		retval = -EINVAL;
		goto OUT;
	}

	if (copy_from_user(tmp_buf, buf, count)) {
		BT_LOG_PRT_ERR("copy_from_user failed!\n");
		retval = -EFAULT;
	} else {
		BT_LOG_PRT_INFO("adb input: %s, len %zd\n", tmp_buf, strlen(tmp_buf));
		if (0 == memcmp(tmp_buf, "raw-hex,", strlen("raw-hex,"))) {
			// Skip prefix
			for(i = strlen("raw-hex,"); i < strlen(tmp_buf); i++) {
				if(tmp_buf[i] == ' ')  // get space
					continue;
				else if(tmp_buf[i] == '\r' || tmp_buf[i] =='\n') // get 0x0a('\n') or 0x0d('\r')
					break;
				// Two input char should turn to one byte
				if (ascii_to_hex(tmp_buf[i], &tmp) == 0) {
					if (j%2 == 0)
						tmp_h = tmp;
					else {
						hci_cmd[k] = tmp_h * 16 + tmp;
						BT_LOG_PRT_DBG("hci_cmd[%zd] = 0x%02x\n", k, hci_cmd[k]);
						k++;
					 }
				} else {
					BT_LOG_PRT_ERR("get unexpected char %c\n", tmp_buf[i]);
					retval = -EINVAL;
					goto OUT;
				}
				j++;
			}
		}
		// ONLY send hci cmd when BT func on
		if (!g_bt_on) {
			retval = -EIO;
			BT_LOG_PRT_ERR("BT func off, skip to send hci cmd\n");
		} else
			retval = send_hci_frame(hci_cmd, k);

	}
OUT:

	return retval;
}

static ssize_t fw_log_bt_read(struct file *filp, char __user *buf, size_t len, loff_t *f_pos)
{
	size_t ret = 0;

	ret = connsys_log_read_to_user(CONNLOG_TYPE_BT, buf, len);
	BT_LOG_PRT_DBG("BT F/W log from connsys len %zd\n", ret);
	return ret;
}

static int fw_log_bt_open(struct inode *inode, struct file *file)
{
	BT_LOG_PRT_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	return 0;
}

static int fw_log_bt_close(struct inode *inode, struct file *file)
{
	BT_LOG_PRT_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	return 0;
}

struct file_operations log_fops = {
	.open = fw_log_bt_open,
	.release = fw_log_bt_close,
	.read = fw_log_bt_read,
	.write = fw_log_bt_write,
	.unlocked_ioctl = fw_log_bt_unlocked_ioctl,
	.compat_ioctl = fw_log_bt_compat_ioctl,
	.poll = fw_log_bt_poll
};

int fw_log_bt_init(void)
{
	INT32 alloc_ret = 0;
	INT32 cdv_err = 0;

	connsys_log_init(CONNLOG_TYPE_BT);

	init_waitqueue_head(&BT_log_wq);
	connsys_log_register_event_cb(CONNLOG_TYPE_BT, fw_log_bt_event_cb);
	sema_init(&ioctl_mtx, 1);

	/* Allocate char device */
	alloc_ret = alloc_chrdev_region(&devno, 0, 1, BT_LOG_NODE_NAME);
	if (alloc_ret) {
		BT_LOG_PRT_ERR("Failed to register device numbers\n");
		return alloc_ret;
	}

	cdev_init(&log_cdev, &log_fops);
	log_cdev.owner = THIS_MODULE;

	cdv_err = cdev_add(&log_cdev, devno, 1);
	if (cdv_err)
		goto error;

#if CREATE_NODE_DYNAMIC /* mknod replace */
	log_class = class_create(THIS_MODULE, BT_LOG_NODE_NAME);
	if (IS_ERR(log_class))
		goto error;

	log_dev = device_create(log_class, NULL, devno, NULL, BT_LOG_NODE_NAME);
	if (IS_ERR(log_dev))
		goto error;
#endif

	BT_LOG_PRT_INFO("%s driver(major %d, minor %d) installed\n", BT_LOG_NODE_NAME, MAJOR(devno), MINOR(devno));
	return 0;

error:

#if CREATE_NODE_DYNAMIC
	if (log_dev && !IS_ERR(log_dev)) {
		device_destroy(log_class, devno);
		log_dev = NULL;
	}
	if (log_class && !IS_ERR(log_class)) {
		class_destroy(log_class);
		log_class = NULL;
	}
#endif
	if (cdv_err == 0)
		cdev_del(&log_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(devno, 1);

	return -1;
}

void fw_log_bt_exit(void)
{
	connsys_log_deinit(CONNLOG_TYPE_BT);

	cdev_del(&log_cdev);
	unregister_chrdev_region(devno, 1);

#if CREATE_NODE_DYNAMIC
	if (log_dev && !IS_ERR(log_dev)) {
		device_destroy(log_class, devno);
		log_dev = NULL;
	}
	if (log_class && !IS_ERR(log_class)) {
		class_destroy(log_class);
		log_class = NULL;
	}
#endif
	BT_LOG_PRT_INFO("%s driver removed\n", BT_LOG_NODE_NAME);
}
#endif
