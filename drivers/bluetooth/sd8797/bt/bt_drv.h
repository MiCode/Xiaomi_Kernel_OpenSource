/** @file bt_drv.h
 *  @brief This header file contains global constant/enum definitions,
 *  global variable declaration.
 *
 *  Copyright (C) 2007-2012, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available along with the File in the gpl.txt file or by writing to
 *  the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 *
 */

#ifndef _BT_DRV_H_
#define _BT_DRV_H_

#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/skbuff.h>

#include "hci_wrapper.h"

#ifndef BIT
/** BIT definition */
#define BIT(x) (1UL << (x))
#endif

#ifdef MBT_64BIT
typedef u64 t_ptr;
#else
typedef u32 t_ptr;
#endif

/** Define drv_mode bit */
#define DRV_MODE_BT         BIT(0)
#define DRV_MODE_FM        BIT(1)
#define DRV_MODE_NFC       BIT(2)

/** Define devFeature bit */
#define DEV_FEATURE_BT     BIT(0)
#define DEV_FEATURE_BTAMP     BIT(1)
#define DEV_FEATURE_BLE     BIT(2)
#define DEV_FEATURE_FM     BIT(3)
#define DEV_FEATURE_NFC     BIT(4)

/** Define maximum number of radio func supported */
#define MAX_RADIO_FUNC     4

/** Debug level : Message */
#define	DBG_MSG			BIT(0)
/** Debug level : Fatal */
#define DBG_FATAL		BIT(1)
/** Debug level : Error */
#define DBG_ERROR		BIT(2)
/** Debug level : Data */
#define DBG_DATA		BIT(3)
/** Debug level : Command */
#define DBG_CMD			BIT(4)
/** Debug level : Event */
#define DBG_EVENT		BIT(5)
/** Debug level : Interrupt */
#define DBG_INTR		BIT(6)

/** Debug entry : Data dump */
#define DBG_DAT_D		BIT(16)
/** Debug entry : Data dump */
#define DBG_CMD_D		BIT(17)

/** Debug level : Entry */
#define DBG_ENTRY		BIT(28)
/** Debug level : Warning */
#define DBG_WARN		BIT(29)
/** Debug level : Informative */
#define DBG_INFO		BIT(30)

#ifdef	DEBUG_LEVEL1
extern u32 mbt_drvdbg;

#ifdef	DEBUG_LEVEL2
/** Print informative message */
#define	PRINTM_INFO(msg...)  do {if (mbt_drvdbg & DBG_INFO)  printk(KERN_DEBUG msg); } while (0)
/** Print warning message */
#define	PRINTM_WARN(msg...)  do {if (mbt_drvdbg & DBG_WARN)  printk(KERN_DEBUG msg); } while (0)
/** Print entry message */
#define	PRINTM_ENTRY(msg...) do {if (mbt_drvdbg & DBG_ENTRY) printk(KERN_DEBUG msg); } while (0)
#else
/** Print informative message */
#define	PRINTM_INFO(msg...)  do {} while (0)
/** Print warning message */
#define	PRINTM_WARN(msg...)  do {} while (0)
/** Print entry message */
#define	PRINTM_ENTRY(msg...) do {} while (0)
#endif /* DEBUG_LEVEL2 */

/** Print interrupt message */
#define	PRINTM_INTR(msg...)  do {if (mbt_drvdbg & DBG_INTR)  printk(KERN_DEBUG msg); } while (0)
/** Print event message */
#define	PRINTM_EVENT(msg...) do {if (mbt_drvdbg & DBG_EVENT) printk(KERN_DEBUG msg); } while (0)
/** Print command message */
#define	PRINTM_CMD(msg...)   do {if (mbt_drvdbg & DBG_CMD)   printk(KERN_DEBUG msg); } while (0)
/** Print data message */
#define	PRINTM_DATA(msg...)  do {if (mbt_drvdbg & DBG_DATA)  printk(KERN_DEBUG msg); } while (0)
/** Print error message */
#define	PRINTM_ERROR(msg...) do {if (mbt_drvdbg & DBG_ERROR) printk(KERN_ERR msg); } while (0)
/** Print fatal message */
#define	PRINTM_FATAL(msg...) do {if (mbt_drvdbg & DBG_FATAL) printk(KERN_ERR msg); } while (0)
/** Print message */
#define	PRINTM_MSG(msg...)   do {if (mbt_drvdbg & DBG_MSG)   printk(KERN_ALERT msg); } while (0)

/** Print data dump message */
#define	PRINTM_DAT_D(msg...)  do {if (mbt_drvdbg & DBG_DAT_D)  printk(KERN_DEBUG msg); } while (0)
/** Print data dump message */
#define	PRINTM_CMD_D(msg...)  do {if (mbt_drvdbg & DBG_CMD_D)  printk(KERN_DEBUG msg); } while (0)

/** Print message with required level */
#define	PRINTM(level, msg...) PRINTM_##level(msg)

/** Debug dump buffer length */
#define DBG_DUMP_BUF_LEN	64
/** Maximum number of dump per line */
#define MAX_DUMP_PER_LINE	16
/** Maximum data dump length */
#define MAX_DATA_DUMP_LEN	48

static inline void
hexdump(char *prompt, u8 * buf, int len)
{
	int i;
	char dbgdumpbuf[DBG_DUMP_BUF_LEN];
	char *ptr = dbgdumpbuf;

	printk(KERN_DEBUG "%s: len=%d\n", prompt, len);
	for (i = 1; i <= len; i++) {
		ptr += snprintf(ptr, 4, "%02x ", *buf);
		buf++;
		if (i % MAX_DUMP_PER_LINE == 0) {
			*ptr = 0;
			printk(KERN_DEBUG "%s\n", dbgdumpbuf);
			ptr = dbgdumpbuf;
		}
	}
	if (len % MAX_DUMP_PER_LINE) {
		*ptr = 0;
		printk(KERN_DEBUG "%s\n", dbgdumpbuf);
	}
}

/** Debug hexdump of debug data */
#define DBG_HEXDUMP_DAT_D(x, y, z)     do {if (mbt_drvdbg & DBG_DAT_D) hexdump(x, y, z); } while (0)
/** Debug hexdump of debug command */
#define DBG_HEXDUMP_CMD_D(x, y, z)     do {if (mbt_drvdbg & DBG_CMD_D) hexdump(x, y, z); } while (0)

/** Debug hexdump */
#define	DBG_HEXDUMP(level, x, y, z)    DBG_HEXDUMP_##level(x, y, z)

/** Mark entry point */
#define	ENTER()			PRINTM(ENTRY, "Enter: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
/** Mark exit point */
#define	LEAVE()			PRINTM(ENTRY, "Leave: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#else
/** Do nothing */
#define	PRINTM(level, msg...) do {} while (0);
/** Do nothing */
#define DBG_HEXDUMP(level, x, y, z)    do {} while (0);
/** Do nothing */
#define	ENTER()  do {} while (0);
/** Do nothing */
#define	LEAVE()  do {} while (0);
#endif /* DEBUG_LEVEL1 */

/** Bluetooth upload size */
#define	BT_UPLD_SIZE				2312
/** Bluetooth status success */
#define BT_STATUS_SUCCESS			(0)
/** Bluetooth status failure */
#define BT_STATUS_FAILURE			(-1)

#ifndef	TRUE
/** True value */
#define TRUE			1
#endif
#ifndef	FALSE
/** False value */
#define	FALSE			0
#endif

/** Set thread state */
#define OS_SET_THREAD_STATE(x)		set_current_state(x)
/** Time to wait until Host Sleep state change in millisecond */
#define WAIT_UNTIL_HS_STATE_CHANGED 2000
/** Time to wait cmd resp in millisecond */
#define WAIT_UNTIL_CMD_RESP	    5000

/** Sleep until a condition gets true or a timeout elapses */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#define os_wait_interruptible_timeout(waitq, cond, timeout) \
	interruptible_sleep_on_timeout(&waitq, ((timeout) * HZ / 1000))
#else
#define os_wait_interruptible_timeout(waitq, cond, timeout) \
	wait_event_interruptible_timeout(waitq, cond, ((timeout) * HZ / 1000))
#endif

typedef struct
{
	/** Task */
	struct task_struct *task;
	/** Queue */
	wait_queue_head_t waitQ;
	/** PID */
	pid_t pid;
	/** Private structure */
	void *priv;
} bt_thread;

static inline void
bt_activate_thread(bt_thread * thr)
{
	/** Initialize the wait queue */
	init_waitqueue_head(&thr->waitQ);

	/** Record the thread pid */
	thr->pid = current->pid;
}

static inline void
bt_deactivate_thread(bt_thread * thr)
{
	thr->pid = 0;
	return;
}

static inline void
bt_create_thread(int (*btfunc) (void *), bt_thread * thr, char *name)
{
	thr->task = kthread_run(btfunc, thr, "%s", name);
}

static inline int
bt_terminate_thread(bt_thread * thr)
{
	/* Check if the thread is active or not */
	if (!thr->pid)
		return -1;

	kthread_stop(thr->task);
	return 0;
}

static inline void
os_sched_timeout(u32 millisec)
{
	set_current_state(TASK_INTERRUPTIBLE);

	schedule_timeout((millisec * HZ) / 1000);
}

#ifndef __ATTRIB_ALIGN__
#define __ATTRIB_ALIGN__ __attribute__((aligned(4)))
#endif

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__ ((packed))
#endif

/** Data structure for the Marvell Bluetooth device */
typedef struct _bt_dev
{
	/** device name */
	char name[DEV_NAME_LEN];
	/** card pointer */
	void *card;
	/** IO port */
	u32 ioport;

	struct m_dev m_dev[MAX_RADIO_FUNC];

	/** Tx download ready flag */
	u8 tx_dnld_rdy;
	/** Function */
	u8 fn;
	/** Rx unit */
	u8 rx_unit;
	/** Power Save mode : Timeout configuration */
	u16 idle_timeout;
	/** Power Save mode */
	u8 psmode;
	/** Power Save command */
	u8 pscmd;
	/** Host Sleep mode */
	u8 hsmode;
	/** Host Sleep command */
	u8 hscmd;
	/** Low byte is gap, high byte is GPIO */
	u16 gpio_gap;
	/** Host Sleep configuration command */
	u8 hscfgcmd;
	/** Host Send Cmd Flag		 */
	u8 sendcmdflag;
	/** ocf for Send Cmd */
	u16 send_cmd_ocf;
	/** Device Type			*/
	u8 devType;
	/** Device Features    */
	u8 devFeature;
	/** cmd52 function */
	u8 cmd52_func;
	/** cmd52 register */
	u8 cmd52_reg;
	/** cmd52 value */
	u8 cmd52_val;
	/** SDIO pull control command */
	u8 sdio_pull_ctrl;
	/** Low 2 bytes is pullUp, high 2 bytes for pull-down */
	u32 sdio_pull_cfg;
} bt_dev_t, *pbt_dev_t;

typedef struct _bt_adapter
{
	/** Chip revision ID */
	u8 chip_rev;
	/** Surprise removed flag */
	u8 SurpriseRemoved;
	/** IRQ number */
	int irq;
	/** Interrupt counter */
	u32 IntCounter;
	/** Tx packet queue */
	struct sk_buff_head tx_queue;
	/** Pending Tx packet queue */
	struct sk_buff_head pending_queue;
	/** tx lock flag */
	u8 tx_lock;
	/** Power Save mode */
	u8 psmode;
	/** Power Save state */
	u8 ps_state;
	/** Host Sleep state */
	u8 hs_state;
	/** hs skip count */
	u32 hs_skip;
	/** suspend_fail flag */
	u8 suspend_fail;
	/** suspended flag */
	u8 is_suspended;
	/** Number of wakeup tries */
	u8 WakeupTries;
	/** Host Sleep wait queue */
	wait_queue_head_t cmd_wait_q __ATTRIB_ALIGN__;
	/** Host Cmd complet state */
	u8 cmd_complete;
	/** last irq recv */
	u8 irq_recv;
	/** last irq processed */
	u8 irq_done;
	/** sdio int status */
	u8 sd_ireg;
	/** tx pending */
	u32 skb_pending;
/** Version string buffer length */
#define MAX_VER_STR_LEN         128
	/** Driver version */
	u8 drv_ver[MAX_VER_STR_LEN];
	/** Number of command timeout */
	u32 num_cmd_timeout;
} bt_adapter, *pbt_adapter;

/** Length of prov name */
#define PROC_NAME_LEN				32

struct item_data
{
	/** Name */
	char name[PROC_NAME_LEN];
	/** Size */
	u32 size;
	/** Address */
	t_ptr addr;
	/** Offset */
	u32 offset;
	/** Flag */
	u32 flag;
};

struct proc_private_data
{
	/** Name */
	char name[PROC_NAME_LEN];
	/** File flag */
	u32 fileflag;
	/** Buffer size */
	u32 bufsize;
	/** Number of items */
	u32 num_items;
	/** Item data */
	struct item_data *pdata;
	/** Private structure */
	struct _bt_private *pbt;
	/** File operations */
	const struct file_operations *fops;
};

struct device_proc
{
	/** Proc directory entry */
	struct proc_dir_entry *proc_entry;
	/** num of proc files */
	u8 num_proc_files;
	/** pointer to proc_private_data */
	struct proc_private_data *pfiles;
};

/** Private structure for the MV device */
typedef struct _bt_private
{
	/** Bluetooth device */
	bt_dev_t bt_dev;
	/** Adapter */
	bt_adapter *adapter;
	/** Firmware helper */
	const struct firmware *fw_helper;
	/** Firmware */
	const struct firmware *firmware;
	/** Firmware request start time */
	struct timeval req_fw_time;
	/** Hotplug device */
	struct device *hotplug_device;
	/** thread to service interrupts */
	bt_thread MainThread;
	 /** proc data */
	struct device_proc dev_proc[MAX_RADIO_FUNC];
	/** Driver lock */
	spinlock_t driver_lock;
	/** Driver lock flags */
	ulong driver_flags;
	int debug_device_pending;
	int debug_ocf_ogf[2];

} bt_private, *pbt_private;

/** Disable interrupt */
#define OS_INT_DISABLE	spin_lock_irqsave(&priv->driver_lock, priv->driver_flags)
/** Enable interrupt */
#define	OS_INT_RESTORE	spin_unlock_irqrestore(&priv->driver_lock, priv->driver_flags)

#ifndef HCI_BT_AMP
/** BT_AMP flag for device type */
#define  HCI_BT_AMP		0x80
#endif

/** Device type of BT */
#define DEV_TYPE_BT		0x00
/** Device type of AMP */
#define DEV_TYPE_AMP		0x01
/** Device type of FM */
#define DEV_TYPE_FM		0x02
/** Device type of NFC */
#define DEV_TYPE_NFC		0x04

/** Marvell vendor packet */
#define MRVL_VENDOR_PKT			0xFE

/** Bluetooth command : Get FW Version */
#define BT_CMD_GET_FW_VERSION       0x0F
/** Bluetooth command : Sleep mode */
#define BT_CMD_AUTO_SLEEP_MODE		0x23
/** Bluetooth command : Host Sleep configuration */
#define BT_CMD_HOST_SLEEP_CONFIG	0x59
/** Bluetooth command : Host Sleep enable */
#define BT_CMD_HOST_SLEEP_ENABLE	0x5A
/** Bluetooth command : Module Configuration request */
#define BT_CMD_MODULE_CFG_REQ		0x5B
/** Bluetooth command : SDIO pull up down configuration request */
#define BT_CMD_SDIO_PULL_CFG_REQ	0x69
#ifdef SDIO_SUSPEND_RESUME
/* FM default event interrupt mask
		bit[0], RSSI low
		bit[1], New RDS data
		bit[2], RSSI indication */
#define FM_DEFAULT_INTR_MASK    0x07
/** Disable FM event interrupt mask */
#define FM_DISABLE_INTR_MASK    0x00
/** FM set event interrupt mask command */
#define FM_SET_INTR_MASK	0x2E
/** FM ocf value */
#define FM_CMD			    0x0280
int fm_set_intr_mask(bt_private * priv, u32 mask);
#endif
/** Sub Command: Module Bring Up Request */
#define MODULE_BRINGUP_REQ		0xF1
/** Sub Command: Module Shut Down Request */
#define MODULE_SHUTDOWN_REQ		0xF2
/** Module already up */
#define MODULE_CFG_RESP_ALREADY_UP      0x0c
/** Sub Command: Host Interface Control Request */
#define MODULE_INTERFACE_CTRL_REQ	0xF5

/** Bluetooth event : Power State */
#define BT_EVENT_POWER_STATE		0x20

/** Bluetooth Power State : Enable */
#define BT_PS_ENABLE			0x02
/** Bluetooth Power State : Disable */
#define BT_PS_DISABLE			0x03
/** Bluetooth Power State : Sleep */
#define BT_PS_SLEEP			0x01
/** Bluetooth Power State : Awake */
#define BT_PS_AWAKE			0x02

/** Vendor OGF */
#define VENDOR_OGF				0x3F
/** OGF for reset */
#define RESET_OGF		0x03
/** Bluetooth command : Reset */
#define BT_CMD_RESET	0x03

/** Host Sleep activated */
#define HS_ACTIVATED			0x01
/** Host Sleep deactivated */
#define HS_DEACTIVATED			0x00

/** Power Save sleep */
#define PS_SLEEP			0x01
/** Power Save awake */
#define PS_AWAKE			0x00

/** bt header length */
#define BT_HEADER_LEN			4

#ifndef MAX
/** Return maximum of two */
#define MAX(a, b)		((a) > (b) ? (a) : (b))
#endif

/** This is for firmware specific length */
#define EXTRA_LEN	36

/** Command buffer size for Marvell driver */
#define MRVDRV_SIZE_OF_CMD_BUFFER       (2 * 1024)

/** Bluetooth Rx packet buffer size for Marvell driver */
#define MRVDRV_BT_RX_PACKET_BUFFER_SIZE \
	(HCI_MAX_FRAME_SIZE + EXTRA_LEN)

/** Buffer size to allocate */
#define ALLOC_BUF_SIZE	(((MAX(MRVDRV_BT_RX_PACKET_BUFFER_SIZE, \
			MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
			+ SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE)

/** Request FW timeout in second */
#define REQUEST_FW_TIMEOUT		30

/** The number of times to try when polling for status bits */
#define MAX_POLL_TRIES			100

/** The number of times to try when waiting for downloaded firmware to
    become active when multiple interface is present */
#define MAX_MULTI_INTERFACE_POLL_TRIES  1000

/** The number of times to try when waiting for downloaded firmware to
     become active. (polling the scratch register). */
#define MAX_FIRMWARE_POLL_TRIES		100

/** default idle time */
#define DEFAULT_IDLE_TIME           1000

typedef struct _BT_CMD
{
	/** OCF OGF */
	u16 ocf_ogf;
	/** Length */
	u8 length;
	/** Data */
	u8 data[32];
} __ATTRIB_PACK__ BT_CMD;

typedef struct _BT_EVENT
{
	/** Event Counter */
	u8 EC;
	/** Length */
	u8 length;
	/** Data */
	u8 data[8];
} BT_EVENT;

/** This function verify the received event pkt */
int check_evtpkt(bt_private * priv, struct sk_buff *skb);

/* Prototype of global function */

/** This function adds the card */
bt_private *bt_add_card(void *card);
/** This function removes the card */
int bt_remove_card(void *card);
/** This function handles the interrupt */
void bt_interrupt(struct m_dev *m_dev);

/** This function creates proc interface directory structure */
int bt_root_proc_init(void);
/** This function removes proc interface directory structure */
int bt_root_proc_remove(void);
/** This function initializes proc entry */
int bt_proc_init(bt_private * priv, struct m_dev *m_dev, int seq);
/** This function removes proc interface */
void bt_proc_remove(bt_private * priv);

/** This function process the received event */
int bt_process_event(bt_private * priv, struct sk_buff *skb);
/** This function enables host sleep */
int bt_enable_hs(bt_private * priv);
/** This function used to send command to firmware */
int bt_prepare_command(bt_private * priv);
/** This function frees the structure of adapter */
void bt_free_adapter(bt_private * priv);

/** bt driver call this function to register to bus driver */
int *sbi_register(void);
/** bt driver call this function to unregister to bus driver */
void sbi_unregister(void);
/** bt driver calls this function to register the device  */
int sbi_register_dev(bt_private * priv);
/** bt driver calls this function to unregister the device */
int sbi_unregister_dev(bt_private * priv);
/** This function initializes firmware */
int sbi_download_fw(bt_private * priv);
/** Configures hardware to quit deep sleep state */
int sbi_wakeup_firmware(bt_private * priv);
/** Module configuration and register device */
int sbi_register_conf_dpc(bt_private * priv);

/** This function is used to send the data/cmd to hardware */
int sbi_host_to_card(bt_private * priv, u8 * payload, u16 nb);
/** This function reads the current interrupt status register */
int sbi_get_int_status(bt_private * priv);

/** This function enables the host interrupts */
int sd_enable_host_int(bt_private * priv);
/** This function disables the host interrupts */
int sd_disable_host_int(bt_private * priv);
/** This function downloads firmware image to the card */
int sd_download_firmware_w_helper(bt_private * priv);

/** Max line length allowed in init config file */
#define MAX_LINE_LEN        256
/** Max MAC address string length allowed */
#define MAX_MAC_ADDR_LEN    18
/** Max register type/offset/value etc. parameter length allowed */
#define MAX_PARAM_LEN       12

/** Bluetooth command : Mac address configuration */
#define BT_CMD_CONFIG_MAC_ADDR		0x22
/** Bluetooth command : Write CSU register */
#define BT_CMD_CSU_WRITE_REG		0x66
/** Bluetooth command : Load calibrate data */
#define BT_CMD_LOAD_CONFIG_DATA     0x61

/** Bluetooth command : BLE deepsleep */
#define BT_CMD_BLE_DEEP_SLEEP       0x8b

typedef struct _BT_BLE_CMD
{
	/** OCF OGF */
	u16 ocf_ogf;
	/** Length */
	u8 length;
	/** deepsleep flag */
	u8 deepsleep;
} __ATTRIB_PACK__ BT_BLE_CMD;

typedef struct _BT_CSU_CMD
{
	/** OCF OGF */
	u16 ocf_ogf;
	/** Length */
	u8 length;
	/** reg type */
	u8 type;
	/** address */
	u8 offset[4];
	/** Data */
	u8 value[2];
} __ATTRIB_PACK__ BT_CSU_CMD;

/** This function sets mac address */
int bt_set_mac_address(bt_private * priv, u8 * mac);
/** This function writes value to CSU registers */
int bt_write_reg(bt_private * priv, u8 type, u32 offset, u16 value);
/** BT set user defined init data and param */
int bt_init_config(bt_private * priv, char *cfg_file);
/** This function load the calibrate data */
int bt_load_cal_data(bt_private * priv, u8 * config_data, u8 * mac);
/** BT set user defined calibration data */
int bt_cal_config(bt_private * priv, char *cfg_file, char *mac);

typedef struct _BT_HCI_CMD
{
	/** OCF OGF */
	u16 ocf_ogf;
	/** Length */
	u8 length;
	/** cmd type */
	u8 cmd_type;
	/** cmd len */
	u8 cmd_len;
	/** Data */
	u8 data[6];
} __ATTRIB_PACK__ BT_HCI_CMD;

#endif /* _BT_DRV_H_ */
