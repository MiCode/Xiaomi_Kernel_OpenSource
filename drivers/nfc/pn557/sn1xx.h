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
 */
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2019 NXP Semiconductors
 *   *
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
 ******************************************************************************/
#ifndef _NXP_NFC_SN1XX_H_
#define _NXP_NFC_SN1XX_H_

/*
 * NFC power control via ioctl
 * SN1XX_SET_PWR(4): enable firmware download via NCI commands
 * SN1XX_SET_PWR(5): power on/reset NFC and ESE
 * SN1XX_SET_PWR(6): disable firmware download via NCI commands
 */
#define SN1XX_SET_PWR      _IOW(NXP_NFC_MAGIC, 0x01, long)

long sn1xx_nfc_ese_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg);
long sn1xx_nfc_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg);
int sn1xx_nfc_probe(struct nfc_dev *nfc_dev);
int sn1xx_nfc_remove(struct nfc_dev *nfc_dev);
#endif //_NXP_NFC_SN1XX_H_
