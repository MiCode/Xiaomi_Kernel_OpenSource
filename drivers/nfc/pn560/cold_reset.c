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
#if IS_ENABLED(CONFIG_NXP_COLD_RESET)

#include <linux/interrupt.h>
#include <linux/delay.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/kthread.h>             //kernel threads
#include "common.h"
#include "cold_reset.h"

struct nfc_dev *nfc_dev_global = NULL;

int nfc_dev_cold_reset_flush(void)
{
	pr_info("%s: enter", __func__);
	nfc_dev_global->release_read = true;
	nfc_dev_global->nfc_disable_intr(nfc_dev_global);
	wake_up(&nfc_dev_global->read_wq);
	pr_info("%s: exit", __func__);
	return 0;
}

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
	pr_info("%s: enter \n", __func__);

	ret = i2c_master_recv(nfc_dev->i2c_dev.client, buf, hdr_len);
	if (ret < 0) {
		pr_info("%s: returned header error1%d\n", __func__, ret);
		pr_err("%s: returned header error %d\n", __func__, ret);
		return -ENOTCONN;
	}
	length_byte = buf[NCI_PAYLOAD_LEN_IDX];
	ret = i2c_master_recv(nfc_dev->i2c_dev.client, buf + hdr_len,
			      length_byte);
	if (ret < 0) {
		pr_info("%s: returned header error %d\n", __func__, ret);
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
	pr_info("%s: enter \n", __func__);

	gpio_set_ven(nfc_dev, 1);
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

int cold_reset_thread_handler(void *pv)
{
	int ret = 0;
	pr_info("THread is running");
	ret = perform_nfcc_initialization(nfc_dev_global);
	while (!nfc_dev_global->release_read) {
		pr_info("THread is running in infinite Loop \n");
		ret = i2c_read(nfc_dev_global, nfc_dev_global->read_kbuf, 258, 0);
	}
 return 0;
}

int nfc_dev_func(struct nfc_dev *nfc_dev)
{
    nfc_dev_global = nfc_dev;
    return 0;
}
#endif
