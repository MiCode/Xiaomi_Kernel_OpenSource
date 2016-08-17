/** @file bt_main.c
  *
  * @brief This file contains the major functions in BlueTooth
  * driver. It includes init, exit, open, close and main
  * thread etc..
  *
  * Copyright (C) 2007-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available along with the File in the gpl.txt file or by writing to
  * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */
/**
  * @mainpage M-BT Linux Driver
  *
  * @section overview_sec Overview
  *
  * The M-BT is a Linux reference driver for Marvell Bluetooth chipset.
  *
  * @section copyright_sec Copyright
  *
  * Copyright (C) 2007-2012, Marvell International Ltd.
  *
  */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>

#include "bt_drv.h"
#include "mbt_char.h"
#include "bt_sdio.h"

/** Version */
#define VERSION "M2614110"

/** Driver version */
static char mbt_driver_version[] = "SD8897-%s-" VERSION "-(" "FP" FPNUM ")"
#ifdef DEBUG_LEVEL2
	"-dbg"
#endif
	" ";

/** Declare and initialize fw_version */
static char fw_version[32] = "0.0.0.p0";

#define AID_SYSTEM        1000	/* system server */

#define AID_BLUETOOTH     1002	/* bluetooth subsystem */

/** Define module name */
#define MODULE_NAME  "bt_fm_nfc"

/** Declaration of chardev class */
static struct class *chardev_class;

/** Interface specific variables */
static int mbtchar_minor = 0;
static int fmchar_minor = 0;
static int nfcchar_minor = 0;
static int debugchar_minor = 0;

/** Default Driver mode */
static int drv_mode = (DRV_MODE_BT | DRV_MODE_FM | DRV_MODE_NFC);

/** BT interface name */
static char *bt_name = NULL;
/** FM interface name */
static char *fm_name = NULL;
/** NFC interface name */
static char *nfc_name = NULL;
/** BT debug interface name */
static char *debug_name = NULL;

/** Firmware flag */
static int fw = 1;
/** default powermode */
static int psmode = 1;
/** Init config file (MAC address, register etc.) */
static char *init_cfg = NULL;
/** Calibration config file (MAC address, init powe etc.) */
static char *cal_cfg = NULL;
/** Init MAC address */
static char *bt_mac = NULL;

/** Setting mbt_drvdbg value based on DEBUG level */
#ifdef DEBUG_LEVEL1
#ifdef DEBUG_LEVEL2
#define DEFAULT_DEBUG_MASK  (0xffffffff & ~DBG_EVENT)
#else
#define DEFAULT_DEBUG_MASK  (DBG_MSG | DBG_FATAL | DBG_ERROR)
#endif /* DEBUG_LEVEL2 */
u32 mbt_drvdbg = DEFAULT_DEBUG_MASK;
#endif

#ifdef SDIO_SUSPEND_RESUME
/** PM keep power */
int mbt_pm_keep_power = 1;
#endif

static int debug_intf = 1;

/** Enable minicard power-up/down */
int minicard_pwrup = 1;
/** Pointer to struct with control hooks */
struct wifi_platform_data *bt_control_data = NULL;

/**
 *  @brief Alloc bt device
 *
 *  @return    pointer to structure mbt_dev or NULL
 */
struct mbt_dev *
alloc_mbt_dev(void)
{
	struct mbt_dev *mbt_dev;
	ENTER();

	mbt_dev = kzalloc(sizeof(struct mbt_dev), GFP_KERNEL);
	if (!mbt_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return mbt_dev;
}

/**
 *  @brief Alloc fm device
 *
 *  @return    pointer to structure fm_dev or NULL
 */
struct fm_dev *
alloc_fm_dev(void)
{
	struct fm_dev *fm_dev;
	ENTER();

	fm_dev = kzalloc(sizeof(struct fm_dev), GFP_KERNEL);
	if (!fm_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return fm_dev;
}

/**
 *  @brief Alloc nfc device
 *
 *  @return    pointer to structure nfc_dev or NULL
 */
struct nfc_dev *
alloc_nfc_dev(void)
{
	struct nfc_dev *nfc_dev;
	ENTER();

	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (!nfc_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return nfc_dev;
}

/**
 *  @brief Alloc debug device
 *
 *  @return    pointer to structure debug_level or NULL
 */
struct debug_dev *
alloc_debug_dev(void)
{
	struct debug_dev *debug_dev;
	ENTER();

	debug_dev = kzalloc(sizeof(struct debug_dev), GFP_KERNEL);
	if (!debug_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return debug_dev;
}

/**
 *  @brief Frees m_dev
 *
 *  @return    N/A
 */
void
free_m_dev(struct m_dev *m_dev)
{
	ENTER();
	kfree(m_dev->dev_pointer);
	LEAVE();
}

/**
 *  @brief This function verify the received event pkt
 *
 *  Event format:
 *  +--------+--------+--------+--------+--------+
 *  | Event  | Length |  ncmd  |      Opcode     |
 *  +--------+--------+--------+--------+--------+
 *  | 1-byte | 1-byte | 1-byte |      2-byte     |
 *  +--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to rx skb
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
check_evtpkt(bt_private * priv, struct sk_buff *skb)
{
	struct hci_event_hdr *hdr = (struct hci_event_hdr *)skb->data;
	struct hci_ev_cmd_complete *ec;
	u16 opcode, ocf;
	u8 ret = BT_STATUS_SUCCESS;
	ENTER();
	if (!priv->bt_dev.sendcmdflag) {
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	if (hdr->evt == HCI_EV_CMD_COMPLETE) {
		ec = (struct hci_ev_cmd_complete *)
			(skb->data + HCI_EVENT_HDR_SIZE);
		opcode = __le16_to_cpu(ec->opcode);
		ocf = hci_opcode_ocf(opcode);
		PRINTM(CMD, "BT: CMD_COMPLTE ocf=0x%x, send_cmd_ocf=0x%x\n",
		       ocf, priv->bt_dev.send_cmd_ocf);
		if (ocf != priv->bt_dev.send_cmd_ocf) {
			ret = BT_STATUS_FAILURE;
			goto exit;
		}
		switch (ocf) {
		case BT_CMD_MODULE_CFG_REQ:
		case BT_CMD_BLE_DEEP_SLEEP:
		case BT_CMD_CONFIG_MAC_ADDR:
		case BT_CMD_CSU_WRITE_REG:
		case BT_CMD_LOAD_CONFIG_DATA:
		case BT_CMD_AUTO_SLEEP_MODE:
		case BT_CMD_HOST_SLEEP_CONFIG:
		case BT_CMD_SDIO_PULL_CFG_REQ:
		case BT_CMD_RESET:
			priv->bt_dev.sendcmdflag = FALSE;
			priv->adapter->cmd_complete = TRUE;
			wake_up_interruptible(&priv->adapter->cmd_wait_q);
			break;
		case BT_CMD_GET_FW_VERSION:
			{
				u8 *pos = (skb->data + HCI_EVENT_HDR_SIZE +
					   sizeof(struct hci_ev_cmd_complete) +
					   1);
				snprintf(fw_version, sizeof(fw_version),
					 "%u.%u.%u.p%u", pos[2], pos[1], pos[0],
					 pos[3]);
				priv->bt_dev.sendcmdflag = FALSE;
				priv->adapter->cmd_complete = TRUE;
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
				break;
			}
#ifdef SDIO_SUSPEND_RESUME
		case FM_CMD:
			{
				u8 *pos =
					(skb->data + HCI_EVENT_HDR_SIZE +
					 sizeof(struct hci_ev_cmd_complete) +
					 1);
				if (*pos == FM_SET_INTR_MASK) {
					priv->bt_dev.sendcmdflag = FALSE;
					priv->adapter->cmd_complete = TRUE;
					wake_up_interruptible(&priv->adapter->
							      cmd_wait_q);
				}
			}
			break;
#endif
		case BT_CMD_HOST_SLEEP_ENABLE:
			priv->bt_dev.sendcmdflag = FALSE;
			break;
		default:
			ret = BT_STATUS_FAILURE;
			break;
		}
	}
exit:
	if (ret == BT_STATUS_SUCCESS)
		kfree_skb(skb);
	LEAVE();
	return ret;
}

/**
 *  @brief This function process the received event
 *
 *  Event format:
 *  +--------+--------+--------+--------+-----+
 *  |   EC   | Length |           Data        |
 *  +--------+--------+--------+--------+-----+
 *  | 1-byte | 1-byte |          n-byte       |
 *  +--------+--------+--------+--------+-----+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to rx skb
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_process_event(bt_private * priv, struct sk_buff *skb)
{
	u8 ret = BT_STATUS_SUCCESS;
	struct m_dev *m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	BT_EVENT *pEvent;

	ENTER();
	pEvent = (BT_EVENT *) skb->data;
	if (pEvent->EC != 0xff) {
		PRINTM(CMD, "BT: Not Marvell Event=0x%x\n", pEvent->EC);
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	switch (pEvent->data[0]) {
	case BT_CMD_AUTO_SLEEP_MODE:
		if (pEvent->data[2] == BT_STATUS_SUCCESS) {
			if (pEvent->data[1] == BT_PS_ENABLE)
				priv->adapter->psmode = 1;
			else
				priv->adapter->psmode = 0;
			PRINTM(CMD, "BT: PS Mode %s:%s\n", m_dev->name,
			       (priv->adapter->psmode) ? "Enable" : "Disable");

		} else {
			PRINTM(CMD, "BT: PS Mode Command Fail %s\n",
			       m_dev->name);
		}
		break;
	case BT_CMD_HOST_SLEEP_CONFIG:
		if (pEvent->data[3] == BT_STATUS_SUCCESS) {
			PRINTM(CMD, "BT: %s: gpio=0x%x, gap=0x%x\n",
			       m_dev->name, pEvent->data[1], pEvent->data[2]);
		} else {
			PRINTM(CMD, "BT: %s: HSCFG Command Fail\n",
			       m_dev->name);
		}
		break;
	case BT_CMD_HOST_SLEEP_ENABLE:
		if (pEvent->data[1] == BT_STATUS_SUCCESS) {
			priv->adapter->hs_state = HS_ACTIVATED;
			if (priv->adapter->suspend_fail == FALSE) {
#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
#ifdef MMC_PM_FUNC_SUSPENDED
				bt_is_suspended(priv);
#endif
#endif
#endif
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
			}
			if (priv->adapter->psmode)
				priv->adapter->ps_state = PS_SLEEP;
			PRINTM(CMD, "BT: EVENT %s: HS ACTIVATED!\n",
			       m_dev->name);

		} else {
			PRINTM(CMD, "BT: %s: HS Enable Fail\n", m_dev->name);
		}
		break;
	case BT_CMD_MODULE_CFG_REQ:
		if ((priv->bt_dev.sendcmdflag == TRUE) &&
		    ((pEvent->data[1] == MODULE_BRINGUP_REQ)
		     || (pEvent->data[1] == MODULE_SHUTDOWN_REQ))) {
			if (pEvent->data[1] == MODULE_BRINGUP_REQ) {
				PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
				       (pEvent->data[2] && (pEvent->data[2] !=
							    MODULE_CFG_RESP_ALREADY_UP))
				       ? "Bring up Fail" : "Bring up success");
				priv->bt_dev.devType = pEvent->data[3];
				PRINTM(CMD, "devType:%s\n",
				       (pEvent->data[3] ==
					DEV_TYPE_AMP) ? "AMP controller" :
				       "BR/EDR controller");
				priv->bt_dev.devFeature = pEvent->data[4];
				PRINTM(CMD,
				       "devFeature:  %s,    %s,    %s,    %s,    %s\n",
				       ((pEvent->
					 data[4] & DEV_FEATURE_BT) ?
					"BT Feature" : "No BT Feature"),
				       ((pEvent->
					 data[4] & DEV_FEATURE_BTAMP) ?
					"BTAMP Feature" : "No BTAMP Feature"),
				       ((pEvent->
					 data[4] & DEV_FEATURE_BLE) ?
					"BLE Feature" : "No BLE Feature"),
				       ((pEvent->
					 data[4] & DEV_FEATURE_FM) ?
					"FM Feature" : "No FM Feature"),
				       ((pEvent->
					 data[4] & DEV_FEATURE_NFC) ?
					"NFC Feature" : "No NFC Feature"));
			}
			if (pEvent->data[1] == MODULE_SHUTDOWN_REQ) {
				PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
				       (pEvent->
					data[2]) ? "Shut down Fail" :
				       "Shut down success");

			}
			if (pEvent->data[2]) {
				priv->bt_dev.sendcmdflag = FALSE;
				priv->adapter->cmd_complete = TRUE;
				wake_up_interruptible(&priv->adapter->
						      cmd_wait_q);
			}
		} else {
			PRINTM(CMD, "BT_CMD_MODULE_CFG_REQ resp for APP\n");
			ret = BT_STATUS_FAILURE;
		}
		break;
	case BT_EVENT_POWER_STATE:
		if (pEvent->data[1] == BT_PS_SLEEP)
			priv->adapter->ps_state = PS_SLEEP;
		PRINTM(CMD, "BT: EVENT %s:%s\n", m_dev->name,
		       (priv->adapter->ps_state) ? "PS_SLEEP" : "PS_AWAKE");

		break;
	case BT_CMD_SDIO_PULL_CFG_REQ:
		if (pEvent->data[pEvent->length - 1] == BT_STATUS_SUCCESS)
			PRINTM(CMD, "BT: %s: SDIO pull configuration success\n",
			       m_dev->name);

		else {
			PRINTM(CMD, "BT: %s: SDIO pull configuration fail\n",
			       m_dev->name);

		}
		break;
	default:
		PRINTM(CMD, "BT: Unknown Event=%d %s\n", pEvent->data[0],
		       m_dev->name);
		ret = BT_STATUS_FAILURE;
		break;
	}
exit:
	if (ret == BT_STATUS_SUCCESS)
		kfree_skb(skb);
	LEAVE();
	return ret;
}

/**
 *  @brief This function shows debug info for timeout of command sending.
 *
 *  @param adapter  A pointer to bt_adapter
 *  @param cmd      Timeout command id
 *
 *  @return         N/A
 */
static void
bt_cmd_timeout_func(bt_adapter * adapter, u16 cmd)
{
	ENTER();

	adapter->num_cmd_timeout++;

	PRINTM(ERROR, "Version = %s\n", adapter->drv_ver);
	PRINTM(ERROR, "Timeout Command id = 0x%x\n", cmd);
	PRINTM(ERROR, "Number of command timeout = %d\n",
	       adapter->num_cmd_timeout);
	PRINTM(ERROR, "Interrupt counter = %d\n", adapter->IntCounter);
	PRINTM(ERROR, "Power Save mode = %d\n", adapter->psmode);
	PRINTM(ERROR, "Power Save state = %d\n", adapter->ps_state);
	PRINTM(ERROR, "Host Sleep state = %d\n", adapter->hs_state);
	PRINTM(ERROR, "hs skip count = %d\n", adapter->hs_skip);
	PRINTM(ERROR, "suspend_fail flag = %d\n", adapter->suspend_fail);
	PRINTM(ERROR, "suspended flag = %d\n", adapter->is_suspended);
	PRINTM(ERROR, "Number of wakeup tries = %d\n", adapter->WakeupTries);
	PRINTM(ERROR, "Host Cmd complet state = %d\n", adapter->cmd_complete);
	PRINTM(ERROR, "Last irq recv = %d\n", adapter->irq_recv);
	PRINTM(ERROR, "Last irq processed = %d\n", adapter->irq_done);
	PRINTM(ERROR, "sdio int status = %d\n", adapter->sd_ireg);
	PRINTM(ERROR, "tx pending = %d\n", adapter->skb_pending);

	LEAVE();
}

/**
 *  @brief This function send reset cmd to firmware
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return 	   BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_reset_command(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_HCI_CMD *) skb->tail;
	pCmd->ocf_ogf = (RESET_OGF << 10) | BT_CMD_RESET;
	pCmd->length = 0x00;
	pCmd->cmd_type = 0x00;
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, 3);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_RESET;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "Queue Reset Command(0x%x)\n", pCmd->ocf_ogf);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Reset timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_RESET);
	} else {
		PRINTM(CMD, "BT: Reset Command done\n");
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends module cfg cmd to firmware
 *
 *  Command format:
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     OCF OGF     | Length |                Data               |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     2-byte      | 1-byte |               4-byte              |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @param subcmd  sub command
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_module_cfg_cmd(bt_private * priv, int subcmd)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "BT: No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_MODULE_CFG_REQ;
	pCmd->length = 1;
	pCmd->data[0] = subcmd;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_MODULE_CFG_REQ;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "Queue module cfg Command(0x%x)\n", pCmd->ocf_ogf);
	wake_up_interruptible(&priv->MainThread.waitQ);
	/*
	   On some Android platforms certain delay is needed for HCI daemon to
	   remove this module and close itself gracefully. Otherwise it hangs.
	   This 10ms delay is a workaround for such platforms as the root cause
	   has not been found yet. */
	mdelay(10);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: module_cfg_cmd (0x%x): "
		       "timeout sendcmdflag=%d\n",
		       subcmd, priv->bt_dev.sendcmdflag);
		bt_cmd_timeout_func(priv->adapter, BT_CMD_MODULE_CFG_REQ);
	} else {
		PRINTM(CMD, "BT: module cfg Command done\n");
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function enables power save mode
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_ps(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_AUTO_SLEEP_MODE;
	if (priv->bt_dev.psmode)
		pCmd->data[0] = BT_PS_ENABLE;
	else
		pCmd->data[0] = BT_PS_DISABLE;
	if (priv->bt_dev.idle_timeout) {
		pCmd->length = 3;
		pCmd->data[1] = (u8) (priv->bt_dev.idle_timeout & 0x00ff);
		pCmd->data[2] = (priv->bt_dev.idle_timeout & 0xff00) >> 8;
	} else {
		pCmd->length = 1;
	}
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue PSMODE Command(0x%x):%d\n", pCmd->ocf_ogf,
	       pCmd->data[0]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_AUTO_SLEEP_MODE;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: psmode timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_AUTO_SLEEP_MODE);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends hscfg command
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_hscfg_cmd(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_HOST_SLEEP_CONFIG;
	pCmd->length = 2;
	pCmd->data[0] = (priv->bt_dev.gpio_gap & 0xff00) >> 8;
	pCmd->data[1] = (u8) (priv->bt_dev.gpio_gap & 0x00ff);
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD, "Queue HSCFG Command(0x%x),gpio=0x%x,gap=0x%x\n",
	       pCmd->ocf_ogf, pCmd->data[0], pCmd->data[1]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_HOST_SLEEP_CONFIG;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: HSCFG timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_HOST_SLEEP_CONFIG);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sends sdio pull ctrl command
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_send_sdio_pull_ctrl_cmd(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_SDIO_PULL_CFG_REQ;
	pCmd->length = 4;
	pCmd->data[0] = (priv->bt_dev.sdio_pull_cfg & 0x000000ff);
	pCmd->data[1] = (priv->bt_dev.sdio_pull_cfg & 0x0000ff00) >> 8;
	pCmd->data[2] = (priv->bt_dev.sdio_pull_cfg & 0x00ff0000) >> 16;
	pCmd->data[3] = (priv->bt_dev.sdio_pull_cfg & 0xff000000) >> 24;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	PRINTM(CMD,
	       "Queue SDIO PULL CFG Command(0x%x), PullUp=0x%x%x,PullDown=0x%x%x\n",
	       pCmd->ocf_ogf, pCmd->data[1], pCmd->data[0],
	       pCmd->data[3], pCmd->data[2]);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_SDIO_PULL_CFG_REQ;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: SDIO PULL CFG timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_SDIO_PULL_CFG_REQ);
	}
exit:
	LEAVE();
	return ret;
}

#ifdef SDIO_SUSPEND_RESUME
/**
 *  @brief This function set FM interrupt mask
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @param priv    FM interrupt mask value
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
fm_set_intr_mask(bt_private * priv, u32 mask)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;

	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | FM_CMD;
	pCmd->length = 0x05;
	pCmd->data[0] = FM_SET_INTR_MASK;
	memcpy(&pCmd->data[1], &mask, sizeof(mask));
	PRINTM(CMD, "FM set intr mask=0x%x\n", mask);
	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[FM_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = FM_CMD;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "FM: set intr mask=%d timeout\n", (int)mask);
		bt_cmd_timeout_func(priv->adapter, FM_CMD);
	}
exit:
	LEAVE();
	return ret;
}
#endif

/**
 *  @brief This function enables host sleep
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_enable_hs(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	priv->adapter->suspend_fail = FALSE;
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_HOST_SLEEP_ENABLE;
	pCmd->length = 0;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_HOST_SLEEP_ENABLE;
	PRINTM(CMD, "Queue hs enable Command(0x%x)\n", pCmd->ocf_ogf);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->hs_state,
	     WAIT_UNTIL_HS_STATE_CHANGED)) {
		PRINTM(MSG, "BT: Enable host sleep timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_HOST_SLEEP_ENABLE);
	}
	OS_INT_DISABLE;
	if ((priv->adapter->hs_state == HS_ACTIVATED) ||
	    (priv->adapter->is_suspended == TRUE)) {
		OS_INT_RESTORE;
		PRINTM(MSG, "BT: suspend success! skip=%d\n",
		       priv->adapter->hs_skip);
	} else {
		priv->adapter->suspend_fail = TRUE;
		OS_INT_RESTORE;
		priv->adapter->hs_skip++;
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG,
		       "BT: suspend skipped! "
		       "state=%d skip=%d ps_state= %d WakeupTries=%d\n",
		       priv->adapter->hs_state, priv->adapter->hs_skip,
		       priv->adapter->ps_state, priv->adapter->WakeupTries);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sets ble deepsleep mode
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mode    TRUE/FALSE
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_ble_deepsleep(bt_private * priv, int mode)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_BLE_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_BLE_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_BLE_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_BLE_DEEP_SLEEP;
	pCmd->length = 1;
	pCmd->deepsleep = mode;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_BLE_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_BLE_DEEP_SLEEP;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set BLE deepsleep = %d (0x%x)\n", mode, pCmd->ocf_ogf);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set BLE deepsleep timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_BLE_DEEP_SLEEP);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function gets FW version
 *
 *  @param priv    A pointer to bt_private structure
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_get_fw_version(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_HCI_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_GET_FW_VERSION;
	pCmd->length = 0x01;
	pCmd->cmd_type = 0x00;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, 4);
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_GET_FW_VERSION;
	priv->adapter->cmd_complete = FALSE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout(priv->adapter->cmd_wait_q,
					   priv->adapter->cmd_complete,
					   WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Get FW version: timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_GET_FW_VERSION);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function sets mac address
 *
 *  @param priv    A pointer to bt_private structure
 *  @param mac     A pointer to mac address
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_set_mac_address(bt_private * priv, u8 * mac)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_HCI_CMD *pCmd;
	int i = 0;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_HCI_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_HCI_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_CONFIG_MAC_ADDR;
	pCmd->length = 8;
	pCmd->cmd_type = MRVL_VENDOR_PKT;
	pCmd->cmd_len = 6;
	for (i = 0; i < 6; i++)
		pCmd->data[i] = mac[5 - i];
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_HCI_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_CONFIG_MAC_ADDR;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set mac addr %02x:%02x:%02x:%02x:%02x:%02x (0x%x)\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], pCmd->ocf_ogf);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(MSG, "BT: Set mac addr: timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_CONFIG_MAC_ADDR);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function load the calibrate data
 *
 *  @param priv    A pointer to bt_private structure
 *  @param config_data     A pointer to calibrate data
 *  @param mac     A pointer to mac address
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_load_cal_data(bt_private * priv, u8 * config_data, u8 * mac)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CMD *pCmd;
	int i = 0;
	// u8 config_data[28] = {0x37 0x01 0x1c 0x00 0xFF 0xFF 0xFF 0xFF
	// 0x01 0x7f 0x04 0x02 0x00 0x00 0xBA 0xCE
	// 0xC0 0xC6 0x2D 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xF0};

	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_LOAD_CONFIG_DATA;
	pCmd->length = 0x20;
	pCmd->data[0] = 0x00;
	pCmd->data[1] = 0x00;
	pCmd->data[2] = 0x00;
	pCmd->data[3] = 0x1C;
	// swip cal-data byte
	for (i = 4; i < 32; i++) {
		pCmd->data[i] = config_data[(i / 4) * 8 - 1 - i];
	}
	if (mac != NULL) {
		pCmd->data[2] = 0x01;	// skip checksum
		for (i = 24; i < 30; i++)
			pCmd->data[i] = mac[29 - i];
	}
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_LOAD_CONFIG_DATA;
	priv->adapter->cmd_complete = FALSE;

	DBG_HEXDUMP(DAT_D, "calirate data: ", pCmd->data, 32);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(ERROR, "BT: Load calibrate data: timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_LOAD_CONFIG_DATA);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function writes value to CSU registers
 *
 *  @param priv    A pointer to bt_private structure
 *  @param type    reg type
 *  @param offset  register address
 *  @param value   register value to write
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_write_reg(bt_private * priv, u8 type, u32 offset, u16 value)
{
	struct sk_buff *skb = NULL;
	u8 ret = BT_STATUS_SUCCESS;
	BT_CSU_CMD *pCmd;
	ENTER();
	skb = bt_skb_alloc(sizeof(BT_CSU_CMD), GFP_ATOMIC);
	if (skb == NULL) {
		PRINTM(WARN, "No free skb\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	pCmd = (BT_CSU_CMD *) skb->data;
	pCmd->ocf_ogf = (VENDOR_OGF << 10) | BT_CMD_CSU_WRITE_REG;
	pCmd->length = 7;
	pCmd->type = type;
	pCmd->offset[0] = (offset & 0x000000ff);
	pCmd->offset[1] = (offset & 0x0000ff00) >> 8;
	pCmd->offset[2] = (offset & 0x00ff0000) >> 16;
	pCmd->offset[3] = (offset & 0xff000000) >> 24;
	pCmd->value[0] = (value & 0x00ff);
	pCmd->value[1] = (value & 0xff00) >> 8;
	bt_cb(skb)->pkt_type = MRVL_VENDOR_PKT;
	skb_put(skb, sizeof(BT_CSU_CMD));
	skb->dev = (void *)(&(priv->bt_dev.m_dev[BT_SEQ]));
	skb_queue_head(&priv->adapter->tx_queue, skb);
	priv->bt_dev.sendcmdflag = TRUE;
	priv->bt_dev.send_cmd_ocf = BT_CMD_CSU_WRITE_REG;
	priv->adapter->cmd_complete = FALSE;
	PRINTM(CMD, "BT: Set CSU reg type=%d reg=0x%x value=0x%x\n",
	       type, offset, value);
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (!os_wait_interruptible_timeout
	    (priv->adapter->cmd_wait_q, priv->adapter->cmd_complete,
	     WAIT_UNTIL_CMD_RESP)) {
		ret = BT_STATUS_FAILURE;
		PRINTM(ERROR, "BT: Set CSU reg timeout:\n");
		bt_cmd_timeout_func(priv->adapter, BT_CMD_CSU_WRITE_REG);
	}
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief This function used to restore tx_queue
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        N/A
 */
void
bt_restore_tx_queue(bt_private * priv)
{
	struct sk_buff *skb = NULL;
	while (!skb_queue_empty(&priv->adapter->pending_queue)) {
		skb = skb_dequeue(&priv->adapter->pending_queue);
		skb_queue_tail(&priv->adapter->tx_queue, skb);
	}
	wake_up_interruptible(&priv->MainThread.waitQ);
}

/**
 *  @brief This function used to send command to firmware
 *
 *  Command format:
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     OCF OGF     | Length |                Data               |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *  |     2-byte      | 1-byte |               4-byte              |
 *  +--------+--------+--------+--------+--------+--------+--------+
 *
 *  @param priv    A pointer to bt_private structure
 *  @return        BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
bt_prepare_command(bt_private * priv)
{
	u8 ret = BT_STATUS_SUCCESS;
	ENTER();
	if (priv->bt_dev.hscfgcmd) {
		priv->bt_dev.hscfgcmd = 0;
		ret = bt_send_hscfg_cmd(priv);
	}
	if (priv->bt_dev.pscmd) {
		priv->bt_dev.pscmd = 0;
		ret = bt_enable_ps(priv);
	}
	if (priv->bt_dev.sdio_pull_ctrl) {
		priv->bt_dev.sdio_pull_ctrl = 0;
		ret = bt_send_sdio_pull_ctrl_cmd(priv);
	}
	if (priv->bt_dev.hscmd) {
		priv->bt_dev.hscmd = 0;
		if (priv->bt_dev.hsmode)
			ret = bt_enable_hs(priv);
		else {
			ret = sbi_wakeup_firmware(priv);
			priv->adapter->hs_state = HS_DEACTIVATED;
		}
	}
	LEAVE();
	return ret;
}

/** @brief This function processes a single packet
 *
 *  @param priv    A pointer to bt_private structure
 *  @param skb     A pointer to skb which includes TX packet
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
SendSinglePacket(bt_private * priv, struct sk_buff *skb)
{
	int ret;
	ENTER();
	if (!skb || !skb->data) {
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	if (!skb->len || ((skb->len + BT_HEADER_LEN) > BT_UPLD_SIZE)) {
		PRINTM(ERROR, "Tx Error: Bad skb length %d : %d\n", skb->len,
		       BT_UPLD_SIZE);
		LEAVE();
		return BT_STATUS_FAILURE;
	}
	if (skb_headroom(skb) < BT_HEADER_LEN) {
		struct sk_buff *tmp = skb;
		skb = skb_realloc_headroom(skb, BT_HEADER_LEN);
		if (!skb) {
			PRINTM(ERROR, "TX error: realloc_headroom failed %d\n",
			       BT_HEADER_LEN);
			skb = tmp;
			LEAVE();
			return BT_STATUS_FAILURE;
		}
		kfree_skb(tmp);
	}
	/* This is SDIO specific header length: byte[3][2][1], * type: byte[0]
	   (HCI_COMMAND = 1, ACL_DATA = 2, SCO_DATA = 3, 0xFE = Vendor) */
	skb_push(skb, BT_HEADER_LEN);
	skb->data[0] = (skb->len & 0x0000ff);
	skb->data[1] = (skb->len & 0x00ff00) >> 8;
	skb->data[2] = (skb->len & 0xff0000) >> 16;
	skb->data[3] = bt_cb(skb)->pkt_type;
	if (bt_cb(skb)->pkt_type == MRVL_VENDOR_PKT)
		PRINTM(CMD, "DNLD_CMD: ocf_ogf=0x%x len=%d\n",
		       *((u16 *) & skb->data[4]), skb->len);
	ret = sbi_host_to_card(priv, skb->data, skb->len);
	LEAVE();
	return ret;
}

/**
 *  @brief This function initializes the adapter structure
 *  and set default value to the member of adapter.
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    N/A
 */
static void
bt_init_adapter(bt_private * priv)
{
	ENTER();
	skb_queue_head_init(&priv->adapter->tx_queue);
	skb_queue_head_init(&priv->adapter->pending_queue);
	priv->adapter->tx_lock = FALSE;
	priv->adapter->ps_state = PS_AWAKE;
	priv->adapter->suspend_fail = FALSE;
	priv->adapter->is_suspended = FALSE;
	priv->adapter->hs_skip = 0;
	priv->adapter->num_cmd_timeout = 0;
	init_waitqueue_head(&priv->adapter->cmd_wait_q);
	LEAVE();
}

/**
 *  @brief This function initializes firmware
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_init_fw(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	if (fw == 0) {
		sd_enable_host_int(priv);
		goto done;
	}
	sd_disable_host_int(priv);
	if (sbi_download_fw(priv)) {
		PRINTM(ERROR, " FW failed to be download!\n");
		ret = BT_STATUS_FAILURE;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function frees the structure of adapter
 *
 *  @param priv    A pointer to bt_private structure
 *  @return    N/A
 */
void
bt_free_adapter(bt_private * priv)
{
	bt_adapter *Adapter = priv->adapter;
	ENTER();
	skb_queue_purge(&priv->adapter->tx_queue);
	/* Free the adapter object itself */
	kfree(Adapter);
	priv->adapter = NULL;

	LEAVE();
}

/**
 *  @brief This function handles the wrapper_dev ioctl
 *
 *  @param hev     A pointer to wrapper_dev structure
 *  @cmd            ioctl cmd
 *  @arg            argument
 *  @return    -ENOIOCTLCMD
 */
static int
mdev_ioctl(struct m_dev *m_dev, unsigned int cmd, unsigned long arg)
{
	ENTER();
	LEAVE();
	return -ENOIOCTLCMD;
}

/**
 *  @brief This function handles wrapper device destruct
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    N/A
 */
static void
mdev_destruct(struct m_dev *m_dev)
{
	ENTER();
	LEAVE();
	return;
}

/**
 *  @brief This function handles the wrapper device transmit
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @param skb     A pointer to sk_buff structure
 *
 *  @return    BT_STATUS_SUCCESS or other error no.
 */
static int
mdev_send_frame(struct m_dev *m_dev, struct sk_buff *skb)
{
	bt_private *priv = NULL;

	ENTER();
	if (!m_dev || !m_dev->driver_data) {
		PRINTM(ERROR, "Frame for unknown HCI device (m_dev=NULL)\n");
		LEAVE();
		return -ENODEV;
	}
	priv = (bt_private *) m_dev->driver_data;
	if (!test_bit(HCI_RUNNING, &m_dev->flags)) {
		PRINTM(ERROR, "Fail test HCI_RUNNING, flag=0x%lx\n",
		       m_dev->flags);
		LEAVE();
		return -EBUSY;
	}
	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		m_dev->stat.cmd_tx++;
		break;
	case HCI_ACLDATA_PKT:
		m_dev->stat.acl_tx++;
		break;
	case HCI_SCODATA_PKT:
		m_dev->stat.sco_tx++;
		break;
	}

	if (m_dev->dev_type == DEBUG_TYPE) {
		/* remember the ogf_ocf */
		priv->debug_device_pending = 1;
		priv->debug_ocf_ogf[0] = skb->data[0];
		priv->debug_ocf_ogf[1] = skb->data[1];
		PRINTM(CMD, "debug_ocf_ogf[0]=0x%x debug_ocf_ogf[1]=0x%x \n",
		       priv->debug_ocf_ogf[0], priv->debug_ocf_ogf[1]);
	}

	if (priv->adapter->tx_lock == TRUE)
		skb_queue_tail(&priv->adapter->pending_queue, skb);
	else
		skb_queue_tail(&priv->adapter->tx_queue, skb);
	wake_up_interruptible(&priv->MainThread.waitQ);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function flushes the transmit queue
 *
 *  @param m_dev     A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS
 */
static int
mdev_flush(struct m_dev *m_dev)
{
	bt_private *priv = (bt_private *) m_dev->driver_data;
	ENTER();
	skb_queue_purge(&priv->adapter->tx_queue);
	skb_queue_purge(&priv->adapter->pending_queue);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function closes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS
 */
static int
mdev_close(struct m_dev *m_dev)
{
	ENTER();
	mdev_req_lock(m_dev);
	if (!test_and_clear_bit(HCI_UP, &m_dev->flags)) {
		mdev_req_unlock(m_dev);
		LEAVE();
		return 0;
	}

	if (m_dev->flush)
		m_dev->flush(m_dev);
	/* wait up pending read and unregister char dev */
	wake_up_interruptible(&m_dev->req_wait_q);
	/* Drop queues */
	skb_queue_purge(&m_dev->rx_q);

	if (!test_and_clear_bit(HCI_RUNNING, &m_dev->flags)) {
		mdev_req_unlock(m_dev);
		LEAVE();
		return 0;
	}
	module_put(THIS_MODULE);
	m_dev->flags = 0;
	mdev_req_unlock(m_dev);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function opens the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
static int
mdev_open(struct m_dev *m_dev)
{
	ENTER();

	if (try_module_get(THIS_MODULE) == 0)
		return BT_STATUS_FAILURE;

	set_bit(HCI_RUNNING, &m_dev->flags);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function queries the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @param arg     arguement
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
void
mdev_query(struct m_dev *m_dev, unsigned long arg)
{
	struct mbt_dev *mbt_dev = (struct mbt_dev *)m_dev->dev_pointer;

	ENTER();
	if (copy_to_user((void *)arg, &mbt_dev->type, sizeof(mbt_dev->type)))
		PRINTM(ERROR, "IOCTL_QUERY_TYPE: Fail copy to user\n");

	LEAVE();
}

/**
 *  @brief This function initializes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return    BT_STATUS_SUCCESS  or other
 */
void
init_m_dev(struct m_dev *m_dev)
{
	m_dev->dev_pointer = NULL;
	m_dev->driver_data = NULL;
	m_dev->dev_type = 0;
	m_dev->spec_type = 0;
	skb_queue_head_init(&m_dev->rx_q);
	init_waitqueue_head(&m_dev->req_wait_q);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
	init_MUTEX(&m_dev->req_lock);
#else
	sema_init(&m_dev->req_lock, 1);
#endif
	memset(&m_dev->stat, 0, sizeof(struct hci_dev_stats));
	m_dev->open = mdev_open;
	m_dev->close = mdev_close;
	m_dev->flush = mdev_flush;
	m_dev->send = mdev_send_frame;
	m_dev->destruct = mdev_destruct;
	m_dev->ioctl = mdev_ioctl;
	m_dev->query = mdev_query;
	m_dev->owner = THIS_MODULE;

}

/**
 *  @brief This function handles the major job in bluetooth driver.
 *  it handles the event generated by firmware, rx data received
 *  from firmware and tx data sent from kernel.
 *
 *  @param data    A pointer to bt_thread structure
 *  @return        BT_STATUS_SUCCESS
 */
static int
bt_service_main_thread(void *data)
{
	bt_thread *thread = data;
	bt_private *priv = thread->priv;
	bt_adapter *Adapter = priv->adapter;
	wait_queue_t wait;
	struct sk_buff *skb;
	ENTER();
	bt_activate_thread(thread);
	init_waitqueue_entry(&wait, current);
	current->flags |= PF_NOFREEZE;

	for (;;) {
		add_wait_queue(&thread->waitQ, &wait);
		OS_SET_THREAD_STATE(TASK_INTERRUPTIBLE);
		if (priv->adapter->WakeupTries ||
		    ((!priv->adapter->IntCounter) &&
		     (!priv->bt_dev.tx_dnld_rdy ||
		      skb_queue_empty(&priv->adapter->tx_queue)))) {
			PRINTM(INFO, "Main: Thread sleeping...\n");
			schedule();
		}
		OS_SET_THREAD_STATE(TASK_RUNNING);
		remove_wait_queue(&thread->waitQ, &wait);
		if (kthread_should_stop() || Adapter->SurpriseRemoved) {
			PRINTM(INFO, "main-thread: break from main thread: "
			       "SurpriseRemoved=0x%x\n",
			       Adapter->SurpriseRemoved);
			break;
		}

		PRINTM(INFO, "Main: Thread waking up...\n");
		if (priv->adapter->IntCounter) {
			OS_INT_DISABLE;
			Adapter->IntCounter = 0;
			OS_INT_RESTORE;
			sbi_get_int_status(priv);
		} else if ((priv->adapter->ps_state == PS_SLEEP) &&
			   !skb_queue_empty(&priv->adapter->tx_queue)) {
			priv->adapter->WakeupTries++;
			sbi_wakeup_firmware(priv);
			continue;
		}
		if (priv->adapter->ps_state == PS_SLEEP)
			continue;
		if (priv->bt_dev.tx_dnld_rdy == TRUE) {
			if (!skb_queue_empty(&priv->adapter->tx_queue)) {
				skb = skb_dequeue(&priv->adapter->tx_queue);
				if (skb) {
					if (SendSinglePacket(priv, skb))
						((struct m_dev *)skb->dev)->
							stat.err_tx++;
					else
						((struct m_dev *)skb->dev)->
							stat.byte_tx +=
							skb->len;
					kfree_skb(skb);
				}
			}
		}
	}
	bt_deactivate_thread(thread);
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the interrupt. it will change PS
 *  state if applicable. it will wake up main_thread to handle
 *  the interrupt event as well.
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @return        N/A
 */
void
bt_interrupt(struct m_dev *m_dev)
{
	bt_private *priv = (bt_private *) m_dev->driver_data;
	ENTER();
	if (!priv || !priv->adapter) {
		LEAVE();
		return;
	}
	PRINTM(INTR, "*\n");
	priv->adapter->ps_state = PS_AWAKE;
	if (priv->adapter->hs_state == HS_ACTIVATED) {
		PRINTM(CMD, "BT: %s: HS DEACTIVATED in ISR!\n", m_dev->name);
		priv->adapter->hs_state = HS_DEACTIVATED;
	}
	priv->adapter->WakeupTries = 0;
	priv->adapter->IntCounter++;
	wake_up_interruptible(&priv->MainThread.waitQ);
	LEAVE();
}

/**
 *  @brief Module configuration and register device
 *
 *  @param priv      A Pointer to bt_private structure
 *  @return      BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_register_conf_dpc(bt_private * priv)
{
	int ret = BT_STATUS_SUCCESS;
	struct mbt_dev *mbt_dev = NULL;
	struct fm_dev *fm_dev = NULL;
	struct nfc_dev *nfc_dev = NULL;
	struct debug_dev *debug_dev = NULL;
	struct m_dev *m_dev = NULL;
	int i = 0;
	struct char_dev *char_dev = NULL;
	char dev_file[DEV_NAME_LEN + 5];
	unsigned char dev_type = 0;

	ENTER();

	priv->bt_dev.tx_dnld_rdy = TRUE;

	if (drv_mode & DRV_MODE_BT) {
		mbt_dev = alloc_mbt_dev();
		if (!mbt_dev) {
			PRINTM(FATAL, "Can not allocate mbt dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		init_m_dev(&(priv->bt_dev.m_dev[BT_SEQ]));
		priv->bt_dev.m_dev[BT_SEQ].dev_type = BT_TYPE;
		priv->bt_dev.m_dev[BT_SEQ].spec_type = IANYWHERE_SPEC;
		priv->bt_dev.m_dev[BT_SEQ].dev_pointer = (void *)mbt_dev;
		priv->bt_dev.m_dev[BT_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[BT_SEQ].read_continue_flag = 0;
	}

	dev_type = HCI_SDIO;

	if (mbt_dev)
		mbt_dev->type = dev_type;

	ret = bt_send_module_cfg_cmd(priv, MODULE_BRINGUP_REQ);
	if (ret < 0) {
		PRINTM(FATAL, "Module cfg command send failed!\n");
		goto done;
	}
	ret = bt_set_ble_deepsleep(priv, TRUE);
	if (ret < 0) {
		PRINTM(FATAL, "Enable BLE deepsleep failed!\n");
		goto done;
	}
	if (psmode) {
		priv->bt_dev.psmode = TRUE;
		priv->bt_dev.idle_timeout = DEFAULT_IDLE_TIME;
		ret = bt_enable_ps(priv);
		if (ret < 0) {
			PRINTM(FATAL, "Enable PS mode failed!\n");
			goto done;
		}
	}
#ifdef SDIO_SUSPEND_RESUME
	priv->bt_dev.gpio_gap = 0xffff;
	ret = bt_send_hscfg_cmd(priv);
	if (ret < 0) {
		PRINTM(FATAL, "Send HSCFG failed!\n");
		goto done;
	}
#endif
	priv->bt_dev.sdio_pull_cfg = 0xffffffff;
	priv->bt_dev.sdio_pull_ctrl = 0;
	wake_up_interruptible(&priv->MainThread.waitQ);
	if (priv->bt_dev.devType == DEV_TYPE_AMP) {
		mbt_dev->type |= HCI_BT_AMP;
		priv->bt_dev.m_dev[BT_SEQ].dev_type = BT_AMP_TYPE;
	}
	/* block all the packet from bluez */
	if (init_cfg || cal_cfg)
		priv->adapter->tx_lock = TRUE;

	if (mbt_dev) {
		/** init mbt_dev */
		mbt_dev->flags = 0;
		mbt_dev->pkt_type = (HCI_DM1 | HCI_DH1 | HCI_HV1);
		mbt_dev->esco_type = (ESCO_HV1);
		mbt_dev->link_mode = (HCI_LM_ACCEPT);

		mbt_dev->idle_timeout = 0;
		mbt_dev->sniff_max_interval = 800;
		mbt_dev->sniff_min_interval = 80;
		for (i = 0; i < 3; i++)
			mbt_dev->reassembly[i] = NULL;
		atomic_set(&mbt_dev->promisc, 0);

		/** alloc char dev node */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = MBTCHAR_MINOR_BASE + mbtchar_minor;
		if (mbt_dev->type & HCI_BT_AMP)
			char_dev->dev_type = BT_AMP_TYPE;
		else
			char_dev->dev_type = BT_TYPE;

		if (bt_name)
			snprintf(mbt_dev->name, sizeof(mbt_dev->name), "%s%d",
				 bt_name, mbtchar_minor);
		else
			snprintf(mbt_dev->name, sizeof(mbt_dev->name),
				 "mbtchar%d", mbtchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", mbt_dev->name);
		mbtchar_minor++;

		/** create BT char device node */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  mbt_dev->name);

		/** chmod & chown for BT char device */
		mbtchar_chown(dev_file, AID_SYSTEM, AID_BLUETOOTH);
		mbtchar_chmod(dev_file, 0666);

		/** register m_dev to BT char device */
		priv->bt_dev.m_dev[BT_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[BT_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[BT_SEQ].name),
			 mbt_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[BT_SEQ]), BT_SEQ);
	}

	if ((drv_mode & DRV_MODE_FM) &&
	    (!(priv->bt_dev.devType == DEV_TYPE_AMP)) &&
	    (priv->bt_dev.devFeature & DEV_FEATURE_FM)) {

		/** alloc fm_dev */
		fm_dev = alloc_fm_dev();
		if (!fm_dev) {
			PRINTM(FATAL, "Can not allocate fm dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[FM_SEQ]));
		priv->bt_dev.m_dev[FM_SEQ].dev_type = FM_TYPE;
		priv->bt_dev.m_dev[FM_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[FM_SEQ].dev_pointer = (void *)fm_dev;
		priv->bt_dev.m_dev[FM_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[FM_SEQ].read_continue_flag = 0;

		/** create char device for FM */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = FMCHAR_MINOR_BASE + fmchar_minor;
		char_dev->dev_type = FM_TYPE;

		if (fm_name)
			snprintf(fm_dev->name, sizeof(fm_dev->name), "%s%d",
				 fm_name, fmchar_minor);
		else
			snprintf(fm_dev->name, sizeof(fm_dev->name),
				 "mfmchar%d", fmchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", fm_dev->name);
		fmchar_minor++;

		/** register char dev */
		register_char_dev(char_dev, chardev_class,
				  MODULE_NAME, fm_dev->name);

		/** chmod for FM char device */
		mbtchar_chmod(dev_file, 0666);

		/** register m_dev to FM char device */
		priv->bt_dev.m_dev[FM_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[FM_SEQ]);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[FM_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[FM_SEQ].name), fm_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[FM_SEQ]), FM_SEQ);
	}

	if ((drv_mode & DRV_MODE_NFC) &&
	    (!(priv->bt_dev.devType == DEV_TYPE_AMP)) &&
	    (priv->bt_dev.devFeature & DEV_FEATURE_NFC)) {

		/** alloc nfc_dev */
		nfc_dev = alloc_nfc_dev();
		if (!nfc_dev) {
			PRINTM(FATAL, "Can not allocate nfc dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[NFC_SEQ]));
		priv->bt_dev.m_dev[NFC_SEQ].dev_type = NFC_TYPE;
		priv->bt_dev.m_dev[NFC_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[NFC_SEQ].dev_pointer = (void *)nfc_dev;
		priv->bt_dev.m_dev[NFC_SEQ].driver_data = priv;
		priv->bt_dev.m_dev[NFC_SEQ].read_continue_flag = 0;

		/** create char device for NFC */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = NFCCHAR_MINOR_BASE + nfcchar_minor;
		char_dev->dev_type = NFC_TYPE;
		if (nfc_name)
			snprintf(nfc_dev->name, sizeof(nfc_dev->name), "%s%d",
				 nfc_name, nfcchar_minor);
		else
			snprintf(nfc_dev->name, sizeof(nfc_dev->name),
				 "mnfcchar%d", nfcchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s", nfc_dev->name);
		nfcchar_minor++;

		/** register char dev */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  nfc_dev->name);

		/** chmod for NFC char device */
		mbtchar_chmod(dev_file, 0666);

		/** register m_dev to NFC char device */
		priv->bt_dev.m_dev[NFC_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[NFC_SEQ]);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[NFC_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[NFC_SEQ].name),
			 nfc_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[NFC_SEQ]), NFC_SEQ);
	}

	if ((debug_intf) &&
	    ((drv_mode & DRV_MODE_BT) ||
	     (drv_mode & DRV_MODE_FM) || (drv_mode & DRV_MODE_NFC))) {
		/** alloc debug_dev */
		debug_dev = alloc_debug_dev();
		if (!debug_dev) {
			PRINTM(FATAL, "Can not allocate debug dev\n");
			ret = -ENOMEM;
			goto err_kmalloc;
		}

		/** init m_dev */
		init_m_dev(&(priv->bt_dev.m_dev[DEBUG_SEQ]));
		priv->bt_dev.m_dev[DEBUG_SEQ].dev_type = DEBUG_TYPE;
		priv->bt_dev.m_dev[DEBUG_SEQ].spec_type = GENERIC_SPEC;
		priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer = (void *)debug_dev;
		priv->bt_dev.m_dev[DEBUG_SEQ].driver_data = priv;

		/** create char device for Debug */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			class_destroy(chardev_class);
			ret = -ENOMEM;
			goto err_kmalloc;
		}
		char_dev->minor = DEBUGCHAR_MINOR_BASE + debugchar_minor;
		char_dev->dev_type = DEBUG_TYPE;
		if (debug_name)
			snprintf(debug_dev->name, sizeof(debug_dev->name),
				 "%s%d", debug_name, debugchar_minor);
		else
			snprintf(debug_dev->name, sizeof(debug_dev->name),
				 "mdebugchar%d", debugchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/%s",
			 debug_dev->name);
		debugchar_minor++;

		/** register char dev */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  debug_dev->name);

		/** chmod for debug char device */
		mbtchar_chmod(dev_file, 0666);

		/** register m_dev to debug char device */
		priv->bt_dev.m_dev[DEBUG_SEQ].index = char_dev->minor;
		char_dev->m_dev = &(priv->bt_dev.m_dev[DEBUG_SEQ]);

		/** create proc device */
		snprintf(priv->bt_dev.m_dev[DEBUG_SEQ].name,
			 sizeof(priv->bt_dev.m_dev[DEBUG_SEQ].name),
			 debug_dev->name);
		bt_proc_init(priv, &(priv->bt_dev.m_dev[DEBUG_SEQ]), DEBUG_SEQ);
	}

	if (init_cfg)
		if (BT_STATUS_SUCCESS != bt_init_config(priv, init_cfg)) {
			PRINTM(FATAL,
			       "BT: Set user init data and param failed\n");
			if (mbt_dev) {
				m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
				/** unregister m_dev to char_dev */
				m_dev->close(m_dev);
				for (i = 0; i < 3; i++)
					kfree_skb(mbt_dev->reassembly[i]);
				/**  unregister m_dev to char_dev */
				chardev_cleanup_one(m_dev, chardev_class);
				free_m_dev(m_dev);
			}
			ret = BT_STATUS_FAILURE;
			goto done;
		}

	if (cal_cfg)
		if (BT_STATUS_SUCCESS != bt_cal_config(priv, cal_cfg, bt_mac)) {
			PRINTM(FATAL, "BT: Set cal data failed\n");
			if (mbt_dev) {
				m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
				/** unregister m_dev to char_dev */
				m_dev->close(m_dev);
				for (i = 0; i < 3; i++)
					kfree_skb(mbt_dev->reassembly[i]);
				/**  unregister m_dev to char_dev */
				chardev_cleanup_one(m_dev, chardev_class);
				free_m_dev(m_dev);
			}
			ret = BT_STATUS_FAILURE;
			goto done;
		}
	if (init_cfg || cal_cfg) {
		priv->adapter->tx_lock = FALSE;
		bt_restore_tx_queue(priv);
	}

	/* Get FW version */
	bt_get_fw_version(priv);
	snprintf(priv->adapter->drv_ver, MAX_VER_STR_LEN,
		 mbt_driver_version, fw_version);

done:
	LEAVE();
	return ret;
err_kmalloc:
	kfree(mbt_dev);
	kfree(fm_dev);
	kfree(nfc_dev);
	kfree(debug_dev);
	LEAVE();
	return ret;
}

/**
 *  @brief This function adds the card. it will probe the
 *  card, allocate the bt_priv and initialize the device.
 *
 *  @param card    A pointer to card
 *  @return        A pointer to bt_private structure
 */

bt_private *
bt_add_card(void *card)
{
	bt_private *priv = NULL;

	ENTER();

	priv = kzalloc(sizeof(bt_private), GFP_KERNEL);
	if (!priv) {
		PRINTM(FATAL, "Can not allocate priv\n");
		LEAVE();
		return NULL;
	}

	/* allocate buffer for bt_adapter */
	if (!(priv->adapter = kzalloc(sizeof(bt_adapter), GFP_KERNEL))) {
		PRINTM(FATAL, "Allocate buffer for bt_adapter failed!\n");
		goto err_kmalloc;
	}

	bt_init_adapter(priv);

	PRINTM(INFO, "Starting kthread...\n");
	priv->MainThread.priv = priv;
	spin_lock_init(&priv->driver_lock);

	bt_create_thread(bt_service_main_thread, &priv->MainThread,
			 "bt_main_service");

	/* wait for mainthread to up */
	while (!priv->MainThread.pid)
		os_sched_timeout(1);

	priv->bt_dev.card = card;

	((struct sdio_mmc_card *)card)->priv = priv;
	priv->adapter->sd_ireg = 0;
	/*
	 * Register the device. Fillup the private data structure with
	 * relevant information from the card and request for the required
	 * IRQ.
	 */
	if (sbi_register_dev(priv) < 0) {
		PRINTM(FATAL, "Failed to register bt device!\n");
		goto err_registerdev;
	}
	if (bt_init_fw(priv)) {
		PRINTM(FATAL, "BT Firmware Init Failed\n");
		goto err_init_fw;
	}
	LEAVE();
	return priv;

err_init_fw:
	PRINTM(INFO, "Unregister device\n");
	sbi_unregister_dev(priv);
err_registerdev:
	((struct sdio_mmc_card *)card)->priv = NULL;
	/* Stop the thread servicing the interrupts */
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid)
		os_sched_timeout(1);
err_kmalloc:
	if (priv->adapter)
		bt_free_adapter(priv);
	kfree(priv);
	LEAVE();
	return NULL;
}

/**
 *  @brief This function removes the card.
 *
 *  @param card    A pointer to card
 *  @return        BT_STATUS_SUCCESS
 */
int
bt_remove_card(void *card)
{
	struct m_dev *m_dev = NULL;
	bt_private *priv = (bt_private *) card;
	int i;

	ENTER();
	if (!priv) {
		LEAVE();
		return BT_STATUS_SUCCESS;
	}
	if (!priv->adapter->SurpriseRemoved) {
		bt_send_reset_command(priv);
		bt_send_module_cfg_cmd(priv, MODULE_SHUTDOWN_REQ);
		/* Disable interrupts on the card */
		sd_disable_host_int(priv);
		priv->adapter->SurpriseRemoved = TRUE;
	}
	wake_up_interruptible(&priv->adapter->cmd_wait_q);
	priv->adapter->SurpriseRemoved = TRUE;
	wake_up_interruptible(&priv->MainThread.waitQ);
	while (priv->MainThread.pid) {
		os_sched_timeout(1);
		wake_up_interruptible(&priv->MainThread.waitQ);
	}

	bt_proc_remove(priv);

	PRINTM(INFO, "Unregister device\n");
	sbi_unregister_dev(priv);

	if (priv->bt_dev.m_dev[BT_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
		if (m_dev->spec_type == IANYWHERE_SPEC) {
			if ((drv_mode & DRV_MODE_BT) && (mbtchar_minor > 0))
				mbtchar_minor--;
			for (i = 0; i < 3; i++)
				kfree_skb(((struct mbt_dev *)
					   (m_dev->dev_pointer))->
					  reassembly[i]);
			/**  unregister m_dev to char_dev */
			if (chardev_class)
				chardev_cleanup_one(m_dev, chardev_class);
			free_m_dev(m_dev);
		}
	}
	if (priv->bt_dev.m_dev[FM_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[FM_SEQ]);
		if ((drv_mode & DRV_MODE_FM) && (fmchar_minor > 0))
			fmchar_minor--;
			/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
	}
	if (priv->bt_dev.m_dev[NFC_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[NFC_SEQ]);
		if ((drv_mode & DRV_MODE_NFC) && (nfcchar_minor > 0))
			nfcchar_minor--;
		/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
	}
	if (priv->bt_dev.m_dev[DEBUG_SEQ].dev_pointer) {
		m_dev = &(priv->bt_dev.m_dev[DEBUG_SEQ]);
		if ((debug_intf) && (debugchar_minor > 0))
			debugchar_minor--;
		/** unregister m_dev to char_dev */
		if (chardev_class)
			chardev_cleanup_one(m_dev, chardev_class);
		free_m_dev(m_dev);
	}
	PRINTM(INFO, "Free Adapter\n");
	bt_free_adapter(priv);
	kfree(priv);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function sets card detect
 *
 *  @param on      card detect status
 *  @return        0
 */
static int
bt_set_carddetect(int on)
{
	PRINTM(MSG, "%s = %d\n", __FUNCTION__, on);
	if (bt_control_data && bt_control_data->set_carddetect)
		bt_control_data->set_carddetect(on);

	return 0;
}

/**
 *  @brief This function sets power
 *
 *  @param on      power status
 *  @return        0
 */
static int
bt_set_power(int on, unsigned long msec)
{
	PRINTM(MSG, "%s = %d\n", __FUNCTION__, on);
	if (bt_control_data && bt_control_data->set_power)
		bt_control_data->set_power(on);

	if (msec)
		mdelay(msec);
	return 0;
}

/**
 *  @brief This function probes the platform-level device
 *
 *  @param pdev    pointer to struct platform_device
 *  @return        0
 */
static int
bt_probe(struct platform_device *pdev)
{
	struct wifi_platform_data *bt_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	ENTER();

	bt_control_data = bt_ctrl;
	bt_set_power(1, 0);	/* Power On */
	bt_set_carddetect(1);	/* CardDetect (0->1) */

	LEAVE();
	return 0;
}

/**
 *  @brief This function removes the platform-level device
 *
 *  @param pdev    pointer to struct platform_device
 *  @return        0
 */
static int
bt_remove(struct platform_device *pdev)
{
	struct wifi_platform_data *bt_ctrl =
		(struct wifi_platform_data *)(pdev->dev.platform_data);

	ENTER();

	bt_control_data = bt_ctrl;
	bt_set_power(0, 0);	/* Power Off */
	bt_set_carddetect(0);	/* CardDetect (1->0) */

	LEAVE();
	return 0;
}

static struct platform_driver bt_device = {
	.probe = bt_probe,
	.remove = bt_remove,
	.driver = {
		   .name = "mrvl_bt",
		   }
};

/**
 *  @brief This function registers the platform-level device to the bus driver
 *
 *  @return        0--success, failure otherwise
 */
static int
bt_add_dev(void)
{
	int ret = 0;

	ENTER();

	if (minicard_pwrup)
		ret = platform_driver_register(&bt_device);

	LEAVE();
	return ret;
}

/**
 *  @brief This function deregisters the platform-level device
 *
 *  @return        N/A
 */
static void
bt_del_dev(void)
{
	ENTER();

	if (minicard_pwrup)
		platform_driver_unregister(&bt_device);

	LEAVE();
}

/**
 *  @brief This function initializes module.
 *
 *  @return    BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
static int
bt_init_module(void)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();
	bt_root_proc_init();
	if (ret)
		goto done;

		/** create char device class */
	chardev_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(chardev_class)) {
		PRINTM(ERROR, "Unable to allocate class\n");
		bt_root_proc_remove();
		ret = PTR_ERR(chardev_class);
		goto done;
	}

	bt_add_dev();
	if (sbi_register() == NULL) {
		ret = BT_STATUS_FAILURE;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function cleans module
 *
 *  @return        N/A
 */
static void
bt_exit_module(void)
{
	ENTER();
	printk(KERN_ERR "+++++ enter func bt_exit_module() +++++\n");

	sbi_unregister();

	bt_root_proc_remove();
	bt_del_dev();
	class_destroy(chardev_class);
	LEAVE();
}

module_init(bt_init_module);
module_exit(bt_exit_module);

MODULE_AUTHOR("Marvell International Ltd.");
MODULE_DESCRIPTION("Marvell Bluetooth Driver Ver. " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
module_param(fw, int, 1);
MODULE_PARM_DESC(fw, "0: Skip firmware download; otherwise: Download firmware");
module_param(psmode, int, 1);
MODULE_PARM_DESC(psmode, "1: Enable powermode; 0: Disable powermode");
#ifdef	DEBUG_LEVEL1
module_param(mbt_drvdbg, uint, 0);
MODULE_PARM_DESC(mbt_drvdbg, "BIT3:DBG_DATA BIT4:DBG_CMD 0xFF:DBG_ALL");
#endif
#ifdef SDIO_SUSPEND_RESUME
module_param(mbt_pm_keep_power, int, 1);
MODULE_PARM_DESC(mbt_pm_keep_power, "1: PM keep power; 0: PM no power");
#endif
module_param(init_cfg, charp, 0);
MODULE_PARM_DESC(init_cfg, "BT init config file name");
module_param(cal_cfg, charp, 0);
MODULE_PARM_DESC(cal_cfg, "BT calibrate file name");
module_param(bt_mac, charp, 0);
MODULE_PARM_DESC(bt_mac, "BT init mac address");
module_param(minicard_pwrup, int, 0);
MODULE_PARM_DESC(minicard_pwrup,
		 "1: Driver load clears PDn/Rst, unload sets (default); 0: Don't do this.");
module_param(drv_mode, int, 0);
MODULE_PARM_DESC(drv_mode, "Bit 0: BT/AMP/BLE; Bit 1: FM; Bit 2: NFC");
module_param(bt_name, charp, 0);
MODULE_PARM_DESC(bt_name, "BT interface name");
module_param(fm_name, charp, 0);
MODULE_PARM_DESC(fm_name, "FM interface name");
module_param(nfc_name, charp, 0);
MODULE_PARM_DESC(nfc_name, "NFC interface name");
module_param(debug_intf, int, 1);
MODULE_PARM_DESC(debug_intf,
		 "1: Enable debug interface; 0: Disable debug interface ");
module_param(debug_name, charp, 0);
MODULE_PARM_DESC(debug_name, "Debug interface name");
