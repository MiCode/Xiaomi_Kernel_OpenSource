/******************************************************************************
 * Copyright 2024 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/

#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)

#include <linux/interrupt.h>
#include <linux/delay.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "common.h"

/**
 * nfc_nci_data_read - This API can be used to read nci data packet.
 *
 * This function is called from nfc Init.
 *
 * @nfc_dev:  the dev structure for driver.
 * @buf:      to copy/get response message.
 * Return: -ENOTCONN for transcieve error
 * No. of bytes read if Success(or no issue)
 */
static int nfc_nci_data_read(struct nfc_dev *nfc_dev, char *buf)
{
	int ret = 0;
	int length_byte = 0;
	unsigned char hdr_len = NCI_HDR_LEN;

	ret = i2c_master_recv(nfc_dev->i2c_dev.client, buf, hdr_len);
	if (ret < 0) {
		pr_err("%s: returned header error %d\n", __func__, ret);
		return -ENOTCONN;
	}
	length_byte = buf[NCI_PAYLOAD_LEN_IDX];
	ret = i2c_master_recv(nfc_dev->i2c_dev.client, buf + hdr_len,
			      length_byte);
	if (ret < 0) {
		pr_err("%s:  returned payload error %d\n", __func__, ret);
		return -ENOTCONN;
	}
	return (hdr_len + length_byte);
}

/**
 * perform_nfcc_initialization - used to send nci cmd through i2c
 *
 * Reset and init commands send to recover NFC
 *
 * @nfc_dev: nfc device data structure
 * Return: -ENOTCONN for transcieve error
 * 0 if Success(or no issue)
 */
static int perform_nfcc_initialization(struct nfc_dev *nfc_dev)
{
	int ret = 0;
	unsigned char cmd_reset_nci[] = { 0x20, 0x00, 0x01, 0x00 };
	unsigned char cmd_init_nci[] = { 0x20, 0x01, 0x02, 0x00, 0x00 };
	unsigned char cmd_read_buff[MAX_NCI_BUFFER_SIZE];

	gpio_set_ven(nfc_dev, 0);
	gpio_set_ven(nfc_dev, 1);
	do {
		msleep(NFC_RST_CMD_READ_DELAY_MS);
		if (nfc_dev->nfc_write(nfc_dev, cmd_reset_nci, sizeof(cmd_reset_nci),
					NO_RETRY) <= 0)
			break;
		msleep(NFC_RST_CMD_READ_DELAY_MS);
		memset(cmd_read_buff, 0x00, sizeof(cmd_read_buff));
		if (nfc_nci_data_read(nfc_dev, cmd_read_buff) <= 0)
			break;
		msleep(NFC_RST_CMD_READ_DELAY_MS);
		memset(cmd_read_buff, 0x00, sizeof(cmd_read_buff));
		if (nfc_nci_data_read(nfc_dev, cmd_read_buff) <= 0)
			break;
		msleep(NFC_RST_CMD_READ_DELAY_MS);
		if (nfc_dev->nfc_write(nfc_dev, cmd_init_nci, sizeof(cmd_init_nci),
					NO_RETRY) <= 0)
			break;
		msleep(NFC_RST_CMD_READ_DELAY_MS);
		memset(cmd_read_buff, 0x00, sizeof(cmd_read_buff));
		if (nfc_nci_data_read(nfc_dev, cmd_read_buff) <= 0)
			break;
		if (cmd_read_buff[0] == 0x40 && cmd_read_buff[1] == 0x01 &&
			cmd_read_buff[3] == 0x00)
			return 0;
	} while (0);
	ret = -ENOTCONN;
	pr_err("%s: no response for nci cmd, ret: %d\n", __func__, ret);
	return ret;
}

/**
 * nfcc_vbat_recovery - To recover the nfcc from unresponse state due to low vbat.
 *
 * send nfcc recovery for NFC_VBAT_MONITOR_MAX_RETRY_COUNT times
 *
 * @nfc_dev: nfc device data structure
 * Return: -ENOTCONN for transcieve error
 * 0 if Success(or no issue)
 */
int nfcc_vbat_recovery(struct nfc_dev *nfc_dev)
{
	int ret = -ENOTCONN;
	unsigned char retrycount = 0;

	do {
		msleep(NFC_RESET_RETRY_DELAY_MS);
		ret = perform_nfcc_initialization(nfc_dev);
		if (ret == 0) {
			pr_info("%s: nfcc recovery is success and vbat irq enabled\n",
				__func__);
			break;
		}
		pr_info("%s: recovering from vbat trigger, count: %d ret: %d\n",
			__func__, retrycount, ret);
		retrycount++;
	} while (retrycount < NFC_VBAT_MONITOR_MAX_RETRY_COUNT);
	return ret;
}

/**
 * nfc_vbat_monitor_irq_handler - Handler function for vbat interrupt
 *
 * Initialises workqueue after disabling interrupt if present
 *
 * @irq: interrupt
 * @dev_id: nfc device pointer
 * Return: Status of type irqreturn_t
 */
irqreturn_t nfc_vbat_monitor_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	struct nfc_dev *nfc_dev = dev_id;

	spin_lock_irqsave(
		&nfc_dev->nfc_vbat_monitor.nfc_vbat_monitor_enabled_lock,
		flags);
	disable_irq_nosync(nfc_dev->nfc_vbat_monitor.irq_num);
	if (!queue_work(nfc_dev->nfc_vbat_monitor.wq,
			&nfc_dev->nfc_vbat_monitor.work))
		pr_debug("%s: queue_work success\n", __func__);
	spin_unlock_irqrestore(
		&nfc_dev->nfc_vbat_monitor.nfc_vbat_monitor_enabled_lock,
		flags);

	return IRQ_HANDLED;
}

/**
 * nfc_vbat_monitor_workqueue_handler - Handler for workqueue of vbat interrupt
 *
 * Enables interrupt workqueue and initialises recovery of nfcc
 *
 * @work: workqueue structure
 */
static void nfc_vbat_monitor_workqueue_handler(struct work_struct *work)
{
	int ret = 0;
	struct nfc_vbat_monitor *nfc_vbat_monitor =
		container_of(work, struct nfc_vbat_monitor, work);
	struct nfc_dev *nfc_dev = container_of(nfc_vbat_monitor, struct nfc_dev,
					       nfc_vbat_monitor);

	pr_debug("%s: read pending status flag: %d\n", __func__,
		 nfc_dev->cold_reset.is_nfc_read_pending);
	nfc_dev->nfc_disable_intr(nfc_dev);
	mutex_lock(&nfc_dev->write_mutex);
	ret = nfcc_vbat_recovery(nfc_dev);
	if (ret == 0)
		enable_irq(nfc_dev->nfc_vbat_monitor.irq_num);
	if (nfc_dev->cold_reset.is_nfc_read_pending == false) {
		if (ret != 0) {
			pr_err("%s nfcc recovery failed, enabling irq",
			       __func__);
			enable_irq(nfc_dev->nfc_vbat_monitor.irq_num);
		}
	} else {
		nfc_dev->nfc_vbat_monitor.vbat_monitor_status = true;
		wake_up(&nfc_dev->read_wq);
	}
	mutex_unlock(&nfc_dev->write_mutex);
}

/**
 * nfc_vbat_monitor_init_workqueue - Workqueue initialiser for vbat interrupt
 *
 * Allocates workqueue and intiialises it
 *
 * @nfc_vbat_monitor: nfc_vbat_monitor structure of platform for nfc
 * Return: 0 if Success
 */
int nfc_vbat_monitor_init_workqueue(struct nfc_vbat_monitor *nfc_vbat_monitor)
{
	nfc_vbat_monitor->wq =
		alloc_workqueue(NFC_VBAT_MONITOR_WORKQUEUE_NAME, 0, 0);

	if (!nfc_vbat_monitor->wq)
		pr_err("%s: failed to allocate workqueue\n", __func__);
	INIT_WORK(&nfc_vbat_monitor->work, nfc_vbat_monitor_workqueue_handler);
	pr_debug("%s: allocated workqueue\n", __func__);
	return 0;
}

/**
 * nfc_vbat_monitor_init - interrupt initialization for vbat handling
 *
 * Configures vbat gpio and assigns an interrupt number, calls for
 * workqueue intialization
 *
 * @nfc_dev: nfc device data structure
 * @nfc_gpio: platform gpio structure
 * @client: i2c client
 * Return: -1 if initialization failed
 * 0 if Success(or no issue)
 */
int nfc_vbat_monitor_init(struct nfc_dev *nfc_dev,
			  struct platform_gpio *nfc_gpio,
			  struct i2c_client *client)
{
	int ret = -1;

	do {
		ret = configure_gpio(nfc_gpio->vbat_irq, GPIO_IRQ);
		if (ret <= 0) {
			pr_err("%s: unable to request nfc vbat gpio [%d]\n", __func__,
				nfc_gpio->vbat_irq);
			break;
		}
		nfc_dev->nfc_vbat_monitor.irq_num = ret;
		/* init mutex and queues */
		spin_lock_init(
			&nfc_dev->nfc_vbat_monitor.nfc_vbat_monitor_enabled_lock);
		nfc_dev->nfc_vbat_monitor.vbat_monitor_status = false;
		ret = request_irq(nfc_dev->nfc_vbat_monitor.irq_num,
				nfc_vbat_monitor_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				nfc_dev->i2c_dev.client->name, nfc_dev);
		if (ret) {
			pr_err("%s: irq request failed\n", __func__);
			break;
		}
		ret = nfc_vbat_monitor_init_workqueue(&nfc_dev->nfc_vbat_monitor);
		if (ret) {
			pr_err("%s: nfc_gpio workqueue init failed\n", __func__);
			break;
		}
		return 0;
	} while (0);
	pr_err("%s: vbat monitor initialization failed, ret:%d\n", __func__, ret);
	return ret;
}

#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
