/*
 *  Atheros Communication Bluetooth HCIATH3K UART protocol
 *
 *  HCIATH3K (HCI Atheros AR300x Protocol) is a Atheros Communication's
 *  power management protocol extension to H4 to support AR300x Bluetooth Chip.
 *
 *  Copyright (c) 2009-2010 Atheros Communications Inc.
 *  Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 *  Acknowledgements:
 *  This file is based on hci_h4.c, which was written
 *  by Maxim Krasnyansky and Marcel Holtmann.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"
#ifdef CONFIG_SERIAL_MSM_HS
#include <mach/msm_serial_hs.h>
#endif

unsigned int enableuartsleep = 1;
module_param(enableuartsleep, uint, 0644);
/*
 * Global variables
 */
/** Global state flags */
static unsigned long flags;

/** Workqueue to respond to change in hostwake line */
static void wakeup_host_work(struct work_struct *work);

/** Transmission timer */
static void bluesleep_tx_timer_expire(unsigned long data);
static DEFINE_TIMER(tx_timer, bluesleep_tx_timer_expire, 0, 0);

/** Lock for state transitions */
static spinlock_t rw_lock;

#define POLARITY_LOW 0
#define POLARITY_HIGH 1

struct bluesleep_info {
	unsigned host_wake;			/* wake up host */
	unsigned ext_wake;			/* wake up device */
	unsigned host_wake_irq;
	int irq_polarity;
};

/* 1 second timeout */
#define TX_TIMER_INTERVAL  1

/* state variable names and bit positions */
#define BT_TXEXPIRED    0x01
#define BT_SLEEPENABLE  0x02
#define BT_SLEEPCMD	0x03

/* global pointer to a single hci device. */
static struct bluesleep_info *bsi;

struct ath_struct {
	struct hci_uart *hu;
	unsigned int cur_sleep;

	struct sk_buff_head txq;
	struct work_struct ctxtsw;
	struct work_struct ws_sleep;
};

static void hsuart_serial_clock_on(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->uart_port;
	BT_DBG("");
	msm_hs_request_clock_on(port);
}

static void hsuart_serial_clock_off(struct tty_struct *tty)
{
	struct uart_state *state = tty->driver_data;
	struct uart_port *port = state->uart_port;
	BT_DBG("");
	msm_hs_request_clock_off(port);
}

static void modify_timer_task(void)
{
	spin_lock(&rw_lock);
	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	clear_bit(BT_TXEXPIRED, &flags);
	spin_unlock(&rw_lock);

}

static int ath_wakeup_ar3k(struct tty_struct *tty)
{
	int status = 0;
	if (test_bit(BT_TXEXPIRED, &flags)) {
		hsuart_serial_clock_on(tty);
		BT_INFO("wakeup device\n");
		gpio_set_value(bsi->ext_wake, 0);
		msleep(20);
		gpio_set_value(bsi->ext_wake, 1);
	}
	modify_timer_task();
	return status;
}

static void wakeup_host_work(struct work_struct *work)
{
	struct ath_struct *ath =
		container_of(work, struct ath_struct, ws_sleep);

	BT_INFO("wake up host");
	if (test_bit(BT_SLEEPENABLE, &flags)) {
		if (test_bit(BT_TXEXPIRED, &flags))
			hsuart_serial_clock_on(ath->hu->tty);
	}
	modify_timer_task();
}

static void ath_hci_uart_work(struct work_struct *work)
{
	int status;
	struct ath_struct *ath;
	struct hci_uart *hu;
	struct tty_struct *tty;

	ath = container_of(work, struct ath_struct, ctxtsw);

	hu = ath->hu;
	tty = hu->tty;

	/* verify and wake up controller */
	if (test_bit(BT_SLEEPENABLE, &flags))
		status = ath_wakeup_ar3k(tty);
	/* Ready to send Data */
	clear_bit(HCI_UART_SENDING, &hu->tx_state);
	hci_uart_tx_wakeup(hu);
}

static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	struct ath_struct *ath = (struct ath_struct *)dev_id;

	schedule_work(&ath->ws_sleep);

	return IRQ_HANDLED;
}

static int ath_bluesleep_gpio_config(struct ath_struct *ath, int on)
{
	int ret = 0;

	BT_INFO("%s config: %d", __func__, on);
	if (!on) {
		if (disable_irq_wake(bsi->host_wake_irq))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
		goto free_host_wake_irq;
	}

	ret = gpio_request(bsi->host_wake, "bt_host_wake");
	if (ret < 0) {
		BT_ERR("failed to request gpio pin %d, error %d\n",
			bsi->host_wake, ret);
		goto gpio_config_failed;
	}

	/* configure host_wake as input */
	ret = gpio_direction_input(bsi->host_wake);
	if (ret < 0) {
		BT_ERR("failed to config GPIO %d as input pin, err %d\n",
			bsi->host_wake, ret);
		goto gpio_host_wake;
	}

	ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
	if (ret < 0) {
		BT_ERR("failed to request gpio pin %d, error %d\n",
			bsi->ext_wake, ret);
		goto gpio_host_wake;
	}

	ret = gpio_direction_output(bsi->ext_wake, 1);
	if (ret < 0) {
		BT_ERR("failed to config GPIO %d as output pin, err %d\n",
			bsi->ext_wake, ret);
		goto gpio_ext_wake;
	}

	gpio_set_value(bsi->ext_wake, 1);

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = (u_long)ath->hu;

	if (bsi->irq_polarity == POLARITY_LOW) {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				"bluetooth hostwake", (void *)ath);
	} else  {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING,
				"bluetooth hostwake", (void *)ath);
	}
	if (ret  < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
		goto delete_timer;
	}

	ret = enable_irq_wake(bsi->host_wake_irq);
	if (ret < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		goto free_host_wake_irq;
	}

	return 0;

free_host_wake_irq:
	free_irq(bsi->host_wake_irq, (void *)ath);
delete_timer:
	del_timer(&tx_timer);
gpio_ext_wake:
	gpio_free(bsi->ext_wake);
gpio_host_wake:
	gpio_free(bsi->host_wake);
gpio_config_failed:
	return ret;
}

/* Initialize protocol */
static int ath_open(struct hci_uart *hu)
{
	struct ath_struct *ath;

	BT_DBG("hu %p, bsi %p", hu, bsi);

	if (!bsi)
		return -EIO;

	ath = kzalloc(sizeof(*ath), GFP_ATOMIC);
	if (!ath)
		return -ENOMEM;

	skb_queue_head_init(&ath->txq);

	hu->priv = ath;
	ath->hu = hu;

	if (ath_bluesleep_gpio_config(ath, 1) < 0) {
		BT_ERR("HCIATH3K GPIO Config failed");
		hu->priv = NULL;
		kfree(ath);
		return -EIO;
	}

	ath->cur_sleep = enableuartsleep;
	if (ath->cur_sleep == 1) {
		set_bit(BT_SLEEPENABLE, &flags);
		modify_timer_task();
	}
	INIT_WORK(&ath->ctxtsw, ath_hci_uart_work);
	INIT_WORK(&ath->ws_sleep, wakeup_host_work);
	return 0;
}

/* Flush protocol data */
static int ath_flush(struct hci_uart *hu)
{
	struct ath_struct *ath = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ath->txq);

	return 0;
}

/* Close protocol */
static int ath_close(struct hci_uart *hu)
{
	struct ath_struct *ath = hu->priv;

	BT_DBG("hu %p", hu);

	skb_queue_purge(&ath->txq);

	cancel_work_sync(&ath->ctxtsw);

	cancel_work_sync(&ath->ws_sleep);

	if (bsi)
		ath_bluesleep_gpio_config(ath, 0);

	hu->priv = NULL;
	kfree(ath);

	return 0;
}

#define HCI_OP_ATH_SLEEP 0xFC04

/* Enqueue frame for transmittion */
static int ath_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct ath_struct *ath = hu->priv;

	BT_DBG("");

	if (bt_cb(skb)->pkt_type == HCI_SCODATA_PKT) {
		kfree_skb(skb);
		return 0;
	}

	/*
	 * Update power management enable flag with parameters of
	 * HCI sleep enable vendor specific HCI command.
	 */
	if (bt_cb(skb)->pkt_type == HCI_COMMAND_PKT) {
		struct hci_command_hdr *hdr = (void *)skb->data;
		if (__le16_to_cpu(hdr->opcode) == HCI_OP_ATH_SLEEP) {
			set_bit(BT_SLEEPCMD, &flags);
			ath->cur_sleep = skb->data[HCI_COMMAND_HDR_SIZE];
		}
	}

	BT_DBG("hu %p skb %p", hu, skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);

	skb_queue_tail(&ath->txq, skb);
	set_bit(HCI_UART_SENDING, &hu->tx_state);

	schedule_work(&ath->ctxtsw);

	return 0;
}

static struct sk_buff *ath_dequeue(struct hci_uart *hu)
{
	struct ath_struct *ath = hu->priv;

	return skb_dequeue(&ath->txq);
}

/* Recv data */
static int ath_recv(struct hci_uart *hu, void *data, int count)
{
	struct ath_struct *ath = hu->priv;
	unsigned int type;

	BT_DBG("");

	if (hci_recv_stream_fragment(hu->hdev, data, count) < 0)
		BT_ERR("Frame Reassembly Failed");

	if (count & test_bit(BT_SLEEPCMD, &flags)) {
		struct sk_buff *skb = hu->hdev->reassembly[0];

		if (!skb) {
			struct { char type; } *pkt;

			/* Start of the frame */
			pkt = data;
			type = pkt->type;
		} else
			type = bt_cb(skb)->pkt_type;

		if (type == HCI_EVENT_PKT) {
			clear_bit(BT_SLEEPCMD, &flags);
			BT_INFO("cur_sleep:%d\n", ath->cur_sleep);
			if (ath->cur_sleep == 1)
				set_bit(BT_SLEEPENABLE, &flags);
			else
				clear_bit(BT_SLEEPENABLE, &flags);
		}
		if (test_bit(BT_SLEEPENABLE, &flags))
			modify_timer_task();
	}
	return count;
}

static void bluesleep_tx_timer_expire(unsigned long data)
{
	struct hci_uart *hu = (struct hci_uart *) data;

	if (!test_bit(BT_SLEEPENABLE, &flags))
		return;
	BT_INFO("Tx timer expired\n");

	set_bit(BT_TXEXPIRED, &flags);
	hsuart_serial_clock_off(hu->tty);
}

static struct hci_uart_proto athp = {
	.id = HCI_UART_ATH3K,
	.open = ath_open,
	.close = ath_close,
	.recv = ath_recv,
	.enqueue = ath_enqueue,
	.dequeue = ath_dequeue,
	.flush = ath_flush,
};

static int __init bluesleep_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	BT_DBG("");

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi) {
		ret = -ENOMEM;
		goto failed;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"gpio_host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->host_wake = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
						"gpio_ext_wake");
	if (!res) {
		BT_ERR("couldn't find ext_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->ext_wake = res->start;

	bsi->host_wake_irq = platform_get_irq_byname(pdev, "host_wake");
	if (bsi->host_wake_irq < 0) {
		BT_ERR("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bsi;
	}

	bsi->irq_polarity = POLARITY_LOW;	/* low edge (falling edge) */

	return 0;

free_bsi:
	kfree(bsi);
	bsi = NULL;
failed:
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	kfree(bsi);
	return 0;
}

static struct platform_driver bluesleep_driver = {
	.remove = bluesleep_remove,
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
	},
};

int __init ath_init(void)
{
	int ret;

	ret = hci_uart_register_proto(&athp);

	if (!ret)
		BT_INFO("HCIATH3K protocol initialized");
	else {
		BT_ERR("HCIATH3K protocol registration failed");
		return ret;
	}
	ret = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (ret)
		return ret;
	return 0;
}

int __exit ath_deinit(void)
{
	platform_driver_unregister(&bluesleep_driver);
	return hci_uart_unregister_proto(&athp);
}
