/******************************************************************************
 * Copyright (C) 2020-2021 NXP
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
#ifndef _COMMON_ESE_H_
#define _COMMON_ESE_H_

#include "common.h"

/* nci prop msg 1st byte */
#define NCI_PROP_MSG_GID			0x0F
#define NCI_PROP_MSG_CMD			(NCI_CMD | NCI_PROP_MSG_GID)
#define NCI_PROP_MSG_RSP			(NCI_RSP | NCI_PROP_MSG_GID)

/* nci prop msg 2nd byte */
#define CLD_RST_OID				0x1E
#define RST_PROT_OID				0x1F

/* nci prop msg 3rd byte */
#define CLD_RST_PAYLOAD_SIZE			0x00
#define RST_PROT_PAYLOAD_SIZE			0x01

/* nci prop msg response length */
#define NCI_PROP_MSG_RSP_LEN			0x04

/* cold reset guard time to allow back to back cold reset after some time */
#define ESE_CLD_RST_GUARD_TIME_MS		(3000)
/* guard time to reboot after reset */
#define ESE_CLD_RST_REBOOT_GUARD_TIME_MS	(50)
/* sources of reset protection and cold reset */
enum reset_source {
	SRC_SPI = 0,
	SRC_NFC = 0x10,
	SRC_OTHER = 0x20,
	SRC_NONE = 0x80,
};

enum ese_ioctl_request {
	ESE_POWER_ON = 0,	/* eSE POWER ON */
	ESE_POWER_OFF,		/* eSE POWER OFF */
	ESE_POWER_STATE,	/* eSE GET POWER STATE */

	/* ese reset requests from eSE service/hal/driver */
	ESE_CLD_RST,		/* eSE COLD RESET */
	ESE_RST_PROT_EN,	/* eSE RESET PROTECTION ENABLE */
	ESE_RST_PROT_DIS,	/* eSE RESET PROTECTION DISABLE */

	/* similar ese reset requests from nfc service/hal/driver */
	ESE_CLD_RST_NFC = ESE_CLD_RST | SRC_NFC,
	ESE_RST_PROT_EN_NFC = ESE_RST_PROT_EN | SRC_NFC,
	ESE_RST_PROT_DIS_NFC = ESE_RST_PROT_DIS | SRC_NFC,

	/* similar ese reset requests from other service/hal/driver */
	ESE_CLD_RST_OTHER = ESE_CLD_RST | SRC_OTHER,
};

#define GET_SRC(arg)			(arg & 0xF0)
#define IS_SRC(arg, src)		(GET_SRC(arg) == src)
#define IS_SRC_SPI(arg)			IS_SRC(arg, SRC_SPI)
#define IS_SRC_NFC(arg)			IS_SRC(arg, SRC_NFC)
#define IS_SRC_OTHER(arg)		IS_SRC(arg, SRC_OTHER)
#define IS_SRC_VALID(arg)		(IS_SRC_SPI(arg) || \
					IS_SRC_NFC(arg) ||  \
					IS_SRC_OTHER(arg))
#define IS_SRC_VALID_PROT(arg)		(IS_SRC_SPI(arg) || \
					IS_SRC_NFC(arg))

#define IS_RST(arg, type)		((arg & 0xF) == type)
#define IS_CLD_RST_REQ(arg)		IS_RST(arg, ESE_CLD_RST)
#define IS_RST_PROT_EN_REQ(arg)		IS_RST(arg, ESE_RST_PROT_EN)
#define IS_RST_PROT_DIS_REQ(arg)	IS_RST(arg, ESE_RST_PROT_DIS)
#define IS_RST_PROT_REQ(arg)		(IS_RST_PROT_EN_REQ(arg) || \
					IS_RST_PROT_DIS_REQ(arg))
/* This macro evaluates to 1 if prop cmd response is received */
#define IS_PROP_CMD_RSP(buf)		((buf[0] == NCI_PROP_MSG_RSP) && \
					((buf[1] == CLD_RST_OID) || \
					(buf[1] == RST_PROT_OID)))

void wakeup_on_prop_rsp(struct nfc_dev *nfc_dev, uint8_t *buf);
int nfc_ese_pwr(struct nfc_dev *nfc_dev, unsigned long arg);
void ese_cold_reset_release(struct nfc_dev *nfc_dev);
void common_ese_init(struct nfc_dev *nfc_dev);
void common_ese_exit(struct nfc_dev *nfc_dev);

#endif /* _COMMON_ESE_H_ */
