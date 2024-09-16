/*
 *  Copyright (c) 2016,2017 MediaTek Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "btmtk_config.h"
#include "btmtk_define.h"
#include "btmtk_uart.h"
#include "btmtk_main.h"

#define LOG TRUE

/*============================================================================*/
/* Global Variable */
/*============================================================================*/
static struct btmtk_dev *g_bdev;

/*============================================================================*/
/* Function Prototype */
/*============================================================================*/
static int btmtk_uart_allocate_memory(void);
extern int btmtk_load_rom_patch_766x(struct hci_dev *);

unsigned long flagss = 0;

/* Allocate Uart-Related memory */
static int btmtk_uart_allocate_memory(void)
{
	if (g_bdev == NULL) {
		g_bdev = kzalloc(sizeof(*g_bdev), GFP_KERNEL);
		if (!g_bdev) {
			BTMTK_ERR("%s: alloc memory fail (g_data)", __func__);
			return -1;
		}
	}
	return 0;
}

int btmtk_cif_send_calibration(struct hci_dev *hdev)
{

	return 0;
}

int btmtk_cif_dispatch_event(struct hci_dev *hdev, struct sk_buff *skb)
{
	return 0;
}

/* Send cmd and Receive evt */
int btmtk_cif_send_cmd(struct hci_dev *hdev, const uint8_t *cmd,
		const int cmd_len, int retry, int endpoint, unsigned long tx_state)
{
	int ret = -1, len = 0;
	struct btmtk_dev *bdev = hci_get_drvdata(hdev);

	BTMTK_DBG_RAW(cmd, cmd_len, "%s, len = %d, send cmd: ", __func__, cmd_len);
	//BTMTK_INFO("%s: tty %p\n", __func__, bdev->tty);
	while(len != cmd_len) {
		ret = bdev->tty->ops->write(bdev->tty, cmd, cmd_len);
		len += ret;
		BTMTK_DBG("%s, len = %d", __func__, len);
	}

	return ret;
}

static int btmtk_uart_send_query_uart_cmd(struct hci_dev *hdev) {
    u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x05, 0x01, 0x04, 0x01, 0x00, 0x02};
	/* To-Do, for event check */
	/* u8 event[] = { 0x04, 0xE4, 0x0a, 0x02, 0x04, 0x06, 0x00, 0x00, 0x02}; */

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);
	BTMTK_INFO("%s done", __func__);
	return 0;
}


/* ------ LDISC part ------ */

/* btmtk_uart_tty_open

 */


/* btmtk_uart_tty_open

 *
 *     Called when line discipline changed to HCI_UART.
 *
 * Arguments:
 *     tty    pointer to tty info structure
 * Return Value:
 *     0 if success, otherwise error code
 */
static int btmtk_uart_tty_open(struct tty_struct *tty)
{
	BTMTK_INFO("%s: tty %p\n", __func__, tty);

	/* Init tty-related operation */
	tty->receive_room = 65536;
	tty->port->low_latency = 1;

	btmtk_uart_allocate_memory();

	tty->disc_data = g_bdev;
	g_bdev->tty = tty;

	/* Flush any pending characters in the driver and line discipline. */

	/* FIXME: why is this needed. Note don't use ldisc_ref here as the
	   open path is before the ldisc is referencable */

	btmtk_allocate_hci_device(g_bdev, HCI_UART);
	g_bdev->stp_cursor = 2;
	g_bdev->stp_dlen = 0;

	/* definition changed!! */
	if (tty->ldisc->ops->flush_buffer)
		tty->ldisc->ops->flush_buffer(tty);

	tty_driver_flush_buffer(tty);

	BTMTK_INFO("%s: tty done %p\n", __func__, tty);

	return 0;
}

/* btmtk_uart_tty_close()
 *
 *    Called when the line discipline is changed to something
 *    else, the tty is closed, or the tty detects a hangup.
 */
static void btmtk_uart_tty_close(struct tty_struct *tty)
{
	btmtk_free_hci_device(g_bdev, HCI_UART);
	BTMTK_INFO("%s: tty %p", __func__, tty);
	return;
}

/*
 * We don't provide read/write/poll interface for user space.
 */
static ssize_t btmtk_uart_tty_read(struct tty_struct *tty, struct file *file,
				 unsigned char *buf, size_t count)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
	return 0;
}

static ssize_t btmtk_uart_tty_write(struct tty_struct *tty, struct file *file,
				 const unsigned char *data, size_t count)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
	return 0;
}

static unsigned int btmtk_uart_tty_poll(struct tty_struct *tty, struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	if (g_bdev->subsys_reset== 1) {
		mask |= POLLIN | POLLRDNORM;                    /* readable */
		BTMTK_INFO("%s: tty %p", __func__, tty);
	}
	return mask;
}

int btmtk_uart_send_wakeup_cmd(struct hci_dev *hdev)
{
    u8 cmd[] = { 0xFF };
	/* To-Do, for event check */
	/*u8 event[] = { 0x80, 0x00};*/

	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_SKIP_VENDOR_EVT);
	return 0;
}


int btmtk_uart_send_set_uart_cmd(struct hci_dev *hdev)
{
	u8 cmd[] = { 0x01, 0x6F, 0xFC, 0x09, 0x01, 0x04, 0x05, 0x00, 0x01, 0x00, 0x10, 0x0E, 0x00};
	/* To-Do, for event check */
	/* u8 event[] = {0x04, 0xE4, 0x06, 0x02, 0x04, 0x02, 0x00, 0x00, 0x01}; */
	btmtk_main_send_cmd(hdev, cmd, sizeof(cmd), BTMTKUART_TX_WAIT_VND_EVT);

	return 0;
}

/* btmtk_uart_tty_ioctl()
 *
 *    Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 *    tty        pointer to tty instance data
 *    file       pointer to open file object for device
 *    cmd        IOCTL command code
 *    arg        argument for IOCTL call (cmd dependent)
 *
 * Return Value:    Command dependent
 */
static int btmtk_uart_tty_ioctl(struct tty_struct *tty, struct file *file,
			      unsigned int cmd, unsigned long arg)
{
	u32 err = 0;
	BTMTK_INFO("%s: tty %p", __func__, tty);

	switch (cmd) {
	case HCIUARTSETPROTO:
		pr_info("<!!> Set low_latency to TRUE <!!>\n");
		tty->port->low_latency = 1;
		break;
	case HCIUARTSETBAUD:
		pr_info("<!!> Set BAUDRATE <!!>\n");
		btmtk_uart_send_set_uart_cmd(g_bdev->hdev);
		btmtk_uart_send_wakeup_cmd(g_bdev->hdev);
		msleep(100);
		return 1;
	case HCIUARTGETBAUD:
		pr_info("<!!> Get BAUDRATE <!!>\n");
		btmtk_uart_send_query_uart_cmd(g_bdev->hdev);
		return 1;
	case HCIUARTSETSTP:
		pr_info("<!!> Set STP mandatory command <!!>\n");
		return 1;
	case HCIUARTLOADPATCH:
		pr_info("<!!> Set HCIUARTLOADPATCH command <!!>\n");
		btmtk_load_rom_patch_766x(g_bdev->hdev);
		return 1;
	default:
		//pr_info("<!!> n_tty_ioctl_helper <!!>\n");
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
		break;
	};

	return err;
}

static void btmtk_uart_tty_receive(struct tty_struct *tty, const u8 *data, char *flags, int count)
{
	int ret = -1;
	struct btmtk_dev *bdev = tty->disc_data;

	BTMTK_DBG_RAW(data, count, "Receive");

	/* add hci device part */
	ret = btmtk_recv(bdev->hdev, data, count);
	if (test_and_clear_bit(BTMTKUART_TX_SKIP_VENDOR_EVT, &bdev->tx_state)) {
		BTMTK_DBG("%s clear bit BTMTKUART_TX_SKIP_VENDOR_EVT", __func__);
		wake_up(&bdev->p_wait_event_q);
		BTMTK_DBG("%s wake_up p_wait_event_q", __func__);
	} else if (ret < 0) {
		BTMTK_ERR("%s, ret = %d", __func__, ret);
	}
}

/* btmtk_uart_tty_wakeup()
 *
 *    Callback for transmit wakeup. Called when low level
 *    device driver can accept more send data.
 *
 * Arguments:        tty    pointer to associated tty instance data
 * Return Value:    None
 */
static void btmtk_uart_tty_wakeup(struct tty_struct *tty)
{
	BTMTK_INFO("%s: tty %p", __func__, tty);
}

static int uart_register(void)
{
	static struct tty_ldisc_ops btmtk_uart_ldisc;
	u32 err = 0;
	BTMTK_INFO("%s", __func__);

	/* Register the tty discipline */
	memset(&btmtk_uart_ldisc, 0, sizeof(btmtk_uart_ldisc));
	btmtk_uart_ldisc.magic = TTY_LDISC_MAGIC;
	btmtk_uart_ldisc.name = "n_mtk";
	btmtk_uart_ldisc.open = btmtk_uart_tty_open;
	btmtk_uart_ldisc.close = btmtk_uart_tty_close;
	btmtk_uart_ldisc.read = btmtk_uart_tty_read;
	btmtk_uart_ldisc.write = btmtk_uart_tty_write;
	btmtk_uart_ldisc.ioctl = btmtk_uart_tty_ioctl;
	btmtk_uart_ldisc.poll = btmtk_uart_tty_poll;
	btmtk_uart_ldisc.receive_buf = btmtk_uart_tty_receive;
	btmtk_uart_ldisc.write_wakeup = btmtk_uart_tty_wakeup;
	btmtk_uart_ldisc.owner = THIS_MODULE;

	err = tty_register_ldisc(N_MTK, &btmtk_uart_ldisc);
	if (err) {
		BTMTK_ERR("MTK line discipline registration failed. (%d)", err);
		return err;
	}

	BTMTK_INFO("%s done", __func__);
	return err;
}
static int uart_deregister(void)
{
	u32 err = 0;
	err = tty_unregister_ldisc(N_MTK);
	if (err) {
		BTMTK_ERR("line discipline registration failed. (%d)", err);
		return err;
	}
	return 0;
}

int btmtk_cif_register(void)
{
	int ret = -1;

	BTMTK_INFO("%s", __func__);
	ret = uart_register();
	if (ret < 0) {
		BTMTK_ERR("*** UART registration fail(%d)! ***", ret);
		return ret;
	}
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}

int btmtk_cif_deregister(void)
{
	int ret = -1;

	BTMTK_INFO("%s", __func__);
	ret = uart_deregister();
	if (ret < 0) {
		BTMTK_ERR("*** UART deregistration fail(%d)! ***", ret);
		return ret;
	}
	BTMTK_INFO("%s: Done", __func__);
	return 0;
}
