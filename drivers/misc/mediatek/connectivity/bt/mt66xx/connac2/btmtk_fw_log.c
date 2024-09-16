// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#define PFX                         "[BTFWLOG]"
#include "btmtk_chip_if.h"
#include "btmtk_main.h"
#include "connsys_debug_utility.h"


MODULE_LICENSE("Dual BSD/GPL");

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define BT_LOG_NODE_NAME   "fw_log_bt"

#define BT_FW_LOG_IOC_MAGIC          (0xfc)
#define BT_FW_LOG_IOCTL_ON_OFF      _IOW(BT_FW_LOG_IOC_MAGIC, 0, int)
#define BT_FW_LOG_IOCTL_SET_LEVEL   _IOW(BT_FW_LOG_IOC_MAGIC, 1, int)
#define BT_FW_LOG_IOCTL_GET_LEVEL   _IOW(BT_FW_LOG_IOC_MAGIC, 2, int)

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/*
 * Flags to mark for BT state and log state.
 */
#define OFF           0x00
#define ON            0xFF

/*
 * BT Logger Tool will turn on/off Firmware Picus log, and set 3 log levels (Low, SQC and Debug)
 * For extention capability, driver does not check the value range.
 *
 * Combine log state and log level to below settings:
 * - 0x00: OFF
 * - 0x01: Low Power
 * - 0x02: SQC
 * - 0x03: Debug
 */
#define DEFAULT_LEVEL 0x02

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
static struct btmtk_dev *g_bdev;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static struct cdev log_cdev;
#if CREATE_NODE_DYNAMIC
static struct class *log_class;
static struct device *log_dev;
#endif
static dev_t devno;

static wait_queue_head_t BT_log_wq;

static struct semaphore ioctl_mtx;

/* Global variables to save BT function and log status */
static uint8_t g_bt_on = OFF;
static uint8_t g_log_on = OFF;
static uint8_t g_log_level = DEFAULT_LEVEL;
static uint8_t g_log_current = OFF;

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static int32_t ascii_to_hex(uint8_t ascii, uint8_t *hex)
{
	int32_t ret = 0;

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

/* btmtk_send_utc_sync_cmd
 *
 *    Send time sync command to FW to synchronize time
 *
 * Arguments:
 *    [IN] hdev     - hci_device as control structure during BT life cycle
 *
 * Return Value:
 *    N/A
 *
 */
int32_t btmtk_send_utc_sync_cmd(void)
{
	struct bt_internal_cmd *p_inter_cmd = &g_bdev->internal_cmd;
	uint8_t cmd[] =  {0x01, 0x6F, 0xFC, 0x01, 0x0C,
			  0xF0, 0x09, 0x00, 0x02,
			  0x00, 0x00, 0x00, 0x00,	/* UTC time second unit */
			  0x00, 0x00, 0x00, 0x00};	/* UTC time microsecond unit*/
	uint32_t sec, usec;
	/* uint8_t evt[] = {0x04, 0xE4, 0x06, 0x02, 0xF0, 0x02, 0x00, 0x02, 0x00}; */

	BTMTK_INFO("[InternalCmd] %s", __func__);
	down(&g_bdev->internal_cmd_sem);
	g_bdev->event_intercept = TRUE;

	connsys_log_get_utc_time(&sec, &usec);
	memcpy(cmd + 9, &sec, sizeof(uint32_t));
	memcpy(cmd + 9 + sizeof(uint32_t), &usec, sizeof(uint32_t));

	p_inter_cmd->waiting_event = 0xE4;
	p_inter_cmd->pending_cmd_opcode = 0xFC6F;
	p_inter_cmd->wmt_opcode = WMT_OPCODE_0XF0;
	p_inter_cmd->result = WMT_EVT_INVALID;

	btmtk_main_send_cmd(g_bdev->hdev, cmd, sizeof(cmd),
				BTMTKUART_TX_WAIT_VND_EVT);

	g_bdev->event_intercept = FALSE;
	up(&g_bdev->internal_cmd_sem);
	BTMTK_INFO("[InternalCmd] %s done, result = %s", __func__, _internal_evt_result(p_inter_cmd->result));
	return 0;
}

/**
 * \brief: set_fw_log
 *
 * \details
 *   Send an MTK vendor specific command to configure Firmware Picus log:
 *   - enable, disable, or set it to a particular level.
 *
 *   The command format is conventional as below:
 *
 *      5D FC = TCI_MTK_DEBUG_VERSION_INFO
 *      04 = Command Length
 *      01 00 01 XX = Refer to table
 *      XX = Log Level
 *
 *   If configured to return a vendor specific event, the event format is as below:
 *
 *      FF = HCI_MTK_TRACE_EVENT
 *      08 = Event Length
 *      FE = TCI_HOST_DRIVER_LOG_EVENT
 *      5D FC = TCI_MTK_DEBUG_VERSION_INFO
 *      00 = Success
 *      01 00 01 XX = Refer to table
 *
 * +----------------------------+-----------------------+--------------+--------------------+
 * | Return Type                | Op Code               | Log Type     | Log Level          |
 * +----------------------------+-----------------------+--------------+--------------------+
 * | 1 byte                     | 1 byte                | 1 byte       | 1 byte             |
 * +----------------------------+-----------------------+--------------+--------------------+
 * | 0x00 Command Complete      | 0x00 Config Picus Log | 0x00 Via HCI | 0x00 Disable       |
 * | 0x01 Vendor Specific Event |                       | 0x01 Via EMI | 0x01 Low Power     |
 * | 0x02 No Event              |                       |              | 0x02 SQC (Default) |
 * |                            |                       |              | 0x03 Debug (Full)  |
 * +----------------------------+-----------------------+--------------+--------------------+
 *
 * \param
 *  @flag: F/W log level
 *
 * \return
 *  0: success; nagtive: fail
 */
static int32_t set_fw_log(uint8_t flag)
{
	int32_t ret;
	uint8_t HCI_CMD_FW_LOG_DEBUG[] = {0x01, 0x5d, 0xfc, 0x04, 0x02, 0x00, 0x01, 0xff};

	if (g_bdev->rst_level != RESET_LEVEL_NONE) {
		BTMTK_WARN("Resetting, skip set_fw_log [%d]", flag);
		return -1;
	}

	HCI_CMD_FW_LOG_DEBUG[7] = flag;
	BTMTK_DBG("hci_cmd: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			HCI_CMD_FW_LOG_DEBUG[0], HCI_CMD_FW_LOG_DEBUG[1],
			HCI_CMD_FW_LOG_DEBUG[2], HCI_CMD_FW_LOG_DEBUG[3],
			HCI_CMD_FW_LOG_DEBUG[4], HCI_CMD_FW_LOG_DEBUG[5],
			HCI_CMD_FW_LOG_DEBUG[6], HCI_CMD_FW_LOG_DEBUG[7]);

	down(&g_bdev->internal_cmd_sem);
	ret = btmtk_main_send_cmd(g_bdev->hdev, HCI_CMD_FW_LOG_DEBUG,
				  sizeof(HCI_CMD_FW_LOG_DEBUG),
				  BTMTKUART_TX_SKIP_VENDOR_EVT);
	up(&g_bdev->internal_cmd_sem);

	if (ret < 0) {
		BTMTK_ERR("Send F/W log cmd failed!\n");
		return ret;
	}
	return 0;
}

static void fw_log_bt_state_cb(enum bt_state state)
{
	uint8_t on_off;

	on_off = (state == FUNC_ON) ? ON : OFF;
	BTMTK_DBG("bt_on(0x%x) state(%d) on_off(0x%x)\n", g_bt_on, state, on_off);

	if (g_bt_on != on_off) {
		// changed
		if (on_off == OFF) { // should turn off
			g_bt_on = OFF;
			BTMTK_INFO("BT func off, no need to send hci cmd\n");
		} else {
			g_bt_on = ON;
			if(g_log_current) {
				set_fw_log(g_log_current);
				btmtk_send_utc_sync_cmd();
			}
		}
	}
}

static long fw_log_bt_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	uint8_t log_tmp = OFF;

	down(&ioctl_mtx);
	switch (cmd) {
		case BT_FW_LOG_IOCTL_ON_OFF:
			/* Connsyslogger daemon dynamically enable/disable Picus log */
			BTMTK_INFO("[ON_OFF]arg(%lu) bt_on(0x%x) log_on(0x%x) level(0x%x) log_cur(0x%x)\n",
				       arg, g_bt_on, g_log_on, g_log_level, g_log_current);
			log_tmp = (arg == 0) ? OFF : ON;
			if (log_tmp == g_log_on) // no change
				break;
			else { // changed
				g_log_on = log_tmp;
				g_log_current = g_log_on & g_log_level;
				if (g_bt_on) {
					retval = set_fw_log(g_log_current);
					btmtk_send_utc_sync_cmd();
				}
			}
			break;
		case BT_FW_LOG_IOCTL_SET_LEVEL:
			/* Connsyslogger daemon dynamically set Picus log level */
			BTMTK_INFO("[SET_LEVEL]arg(%lu) bt_on(0x%x) log_on(0x%x) level(0x%x) log_cur(0x%x)\n",
				       arg, g_bt_on, g_log_on, g_log_level, g_log_current);
			log_tmp = (uint8_t)arg;
			if(log_tmp == g_log_level) // no change
				break;
			else {
				g_log_level = log_tmp;
				g_log_current = g_log_on & g_log_level;
				if (g_bt_on & g_log_on) {
					// driver on and log on
					retval = set_fw_log(g_log_current);
					btmtk_send_utc_sync_cmd();
				}
			}
			break;
		case BT_FW_LOG_IOCTL_GET_LEVEL:
			retval = g_log_level;
			BTMTK_INFO("[GET_LEVEL]return %ld\n", retval);
			break;
		default:
			BTMTK_ERR("Unknown cmd: 0x%08x\n", cmd);
			retval = -EOPNOTSUPP;
			break;
	}

	up(&ioctl_mtx);
	return retval;
}

static long fw_log_bt_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return fw_log_bt_unlocked_ioctl(filp, cmd, arg);
}

static void fw_log_bt_event_cb(void)
{
	BTMTK_DBG("fw_log_bt_event_cb");
	wake_up_interruptible(&BT_log_wq);
}

static unsigned int fw_log_bt_poll(struct file *file, poll_table *wait)
{
	uint32_t mask = 0;

	poll_wait(file, &BT_log_wq, wait);
	if (connsys_log_get_buf_size(CONN_DEBUG_TYPE_BT) > 0) {
		mask = (POLLIN | POLLRDNORM);
	}
	return mask;
}

static ssize_t fw_log_bt_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
#define BT_LOG_BUFFER_SIZE  512

	ssize_t retval;
	static uint8_t tmp_buf[BT_LOG_BUFFER_SIZE];
	uint8_t hci_cmd[1024] = {0};
	uint8_t tmp = 0, tmp_h = 0;
	uint32_t i = 0, j = 0, k = 0;

	if(count >= BT_LOG_BUFFER_SIZE) {
		BTMTK_ERR("write count %zd exceeds max buffer size %d", count, BT_LOG_BUFFER_SIZE);
		retval = -EINVAL;
		goto OUT;
	}

	if (copy_from_user(tmp_buf, buf, count)) {
		BTMTK_ERR("copy_from_user failed!\n");
		retval = -EFAULT;
	} else {
		BTMTK_INFO("adb input: %s, len %zd\n", tmp_buf, strlen(tmp_buf));
		if (0 == memcmp(tmp_buf, "raw-hex,", strlen("raw-hex,"))) {
			/* Skip prefix */
			for(i = strlen("raw-hex,"); i < strlen(tmp_buf); i++) {
				if(tmp_buf[i] == ' ')  // get space
					continue;
				else if(tmp_buf[i] == '\r' || tmp_buf[i] =='\n') // get 0x0a('\n') or 0x0d('\r')
					break;
				/* Two input char should turn into one byte */
				if (ascii_to_hex(tmp_buf[i], &tmp) == 0) {
					if (j%2 == 0)
						tmp_h = tmp;
					else {
						hci_cmd[k] = tmp_h * 16 + tmp;
						BTMTK_DBG("hci_cmd[%u] = 0x%02x\n", k, hci_cmd[k]);
						k++;
					 }
				} else {
					BTMTK_ERR("get unexpected char %c\n", tmp_buf[i]);
					retval = -EINVAL;
					goto OUT;
				}
				j++;
			}
		}
		/* Only send F/W log cmd when BT func on */
		if (g_bt_on) {
			retval = btmtk_main_send_cmd(g_bdev->hdev, hci_cmd, k, BTMTKUART_TX_SKIP_VENDOR_EVT);
		}
		else {
			BTMTK_ERR("BT func off, skip to send F/W log cmd\n");
			retval = -EIO;
		}
	}

OUT:
	return (retval < 0) ? retval : count;
}

static ssize_t fw_log_bt_read(struct file *filp, char __user *buf, size_t len, loff_t *f_pos)
{
	size_t retval = 0;

	retval = connsys_log_read_to_user(CONN_DEBUG_TYPE_BT, buf, len);
	BTMTK_DBG("BT F/W log from Connsys, len %zd\n", retval);
	return retval;
}

static int fw_log_bt_open(struct inode *inode, struct file *file)
{
	BTMTK_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
	return 0;
}

static int fw_log_bt_close(struct inode *inode, struct file *file)
{
	BTMTK_INFO("major %d minor %d (pid %d)\n", imajor(inode), iminor(inode), current->pid);
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

int fw_log_bt_init(struct hci_dev *hdev)
{
	int32_t alloc_err = 0;
	int32_t cdv_err = 0;

	BTMTK_INFO("%s", __func__);
	g_bdev = hci_get_drvdata(hdev);
	connsys_log_init(CONN_DEBUG_TYPE_BT);
	connsys_log_register_event_cb(CONN_DEBUG_TYPE_BT, fw_log_bt_event_cb);

	init_waitqueue_head(&BT_log_wq);
	sema_init(&ioctl_mtx, 1);

	/* Allocate char device */
	alloc_err = alloc_chrdev_region(&devno, 0, 1, BT_LOG_NODE_NAME);
	if (alloc_err) {
		BTMTK_ERR("Failed to register device numbers\n");
		goto alloc_error;
	}

	cdev_init(&log_cdev, &log_fops);
	log_cdev.owner = THIS_MODULE;

	cdv_err = cdev_add(&log_cdev, devno, 1);
	if (cdv_err)
		goto cdv_error;

#if CREATE_NODE_DYNAMIC /* mknod replace */
	log_class = class_create(THIS_MODULE, BT_LOG_NODE_NAME);
	if (IS_ERR(log_class))
		goto create_node_error;

	log_dev = device_create(log_class, NULL, devno, NULL, BT_LOG_NODE_NAME);
	if (IS_ERR(log_dev))
		goto create_node_error;
#endif

	g_bdev->state_change_cb[0] = fw_log_bt_state_cb;
	BTMTK_INFO("%s driver(major %d, minor %d) installed\n",
		       BT_LOG_NODE_NAME, MAJOR(devno), MINOR(devno));

	return 0;

#if CREATE_NODE_DYNAMIC
create_node_error:
	if (log_class && !IS_ERR(log_class)) {
		class_destroy(log_class);
		log_class = NULL;
	}

	cdev_del(&log_cdev);
#endif

cdv_error:
	unregister_chrdev_region(devno, 1);

alloc_error:
	connsys_log_deinit(CONN_DEBUG_TYPE_BT);
	return -1;
}

void fw_log_bt_exit(void)
{
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

	cdev_del(&log_cdev);
	unregister_chrdev_region(devno, 1);

	g_bdev->state_change_cb[0] = NULL;
	connsys_log_deinit(CONN_DEBUG_TYPE_BT);

	BTMTK_INFO("%s driver removed\n", BT_LOG_NODE_NAME);
}

#endif
