/*
 * The original Work has been changed by NXP Semiconductors.
 * Copyright 2013-2019 NXP
 *
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*
 * Copyright (C) 2010 Trusted Logic S.A.
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
 */
#ifndef _NXP_NFC_H_
#define _NXP_NFC_H_
#define NXP_NFC_MAGIC 0xE9

/* Device specific structure */
struct nfc_dev    {
    wait_queue_head_t   read_wq;
    struct mutex        read_mutex;
    struct mutex        ese_status_mutex;
    struct i2c_client   *client;
    struct miscdevice   nfc_device;
    /* NFC GPIO variables */
    unsigned int        irq_gpio;
    unsigned int        ven_gpio;
    unsigned int        firm_gpio;
    unsigned int        ese_pwr_gpio;
    /* NFC_IRQ state */
    bool                irq_enabled;
    spinlock_t          irq_enabled_lock;
    unsigned int        count_irq;
    /* NFC additional parameters for old platforms */
    void *pdata_op;
};

void nfc_disable_irq(struct nfc_dev *nfc_dev);
void nfc_enable_irq(struct nfc_dev *nfc_dev);
void nfc_ese_acquire(struct nfc_dev *nfc_dev);
void nfc_ese_release(struct nfc_dev *nfc_dev);
#endif //_NXP_NFC_H_