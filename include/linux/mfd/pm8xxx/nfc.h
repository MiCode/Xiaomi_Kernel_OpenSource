/* Copyright (c) 2010,2011 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PM8XXX_NFC_H__
#define __PM8XXX_NFC_H__

struct pm8xxx_nfc_device;

#define PM8XXX_NFC_DEV_NAME		"pm8xxx-nfc"

/* masks, flags and status */
#define	PM_NFC_VDDLDO_MON_LEVEL		0x0003
#define	PM_NFC_VPH_PWR_EN		0x0008
#define	PM_NFC_EXT_VDDLDO_EN		0x0010
#define	PM_NFC_EN			0x0020
#define	PM_NFC_LDO_EN			0x0040
#define	PM_NFC_SUPPORT_EN		0x0080

#define	PM_NFC_EXT_EN_HIGH		0x0100
#define	PM_NFC_MBG_EN_HIGH		0x0200
#define	PM_NFC_VDDLDO_OK_HIGH		0x0400
#define	PM_NFC_DTEST1_MODE		0x2000
#define	PM_NFC_ATEST_EN			0x4000
#define	PM_NFC_VDDLDO_MON_EN		0x8000

#define	PM_NFC_CTRL_REQ			(PM_NFC_SUPPORT_EN |\
					PM_NFC_LDO_EN |\
					PM_NFC_EN |\
					PM_NFC_EXT_VDDLDO_EN |\
					PM_NFC_VPH_PWR_EN |\
					PM_NFC_VDDLDO_MON_LEVEL)

#define	PM_NFC_TEST_REQ			(PM_NFC_VDDLDO_MON_EN |\
					PM_NFC_DTEST1_MODE |\
					PM_NFC_ATEST_EN)

#define	PM_NFC_TEST_STATUS		(PM_NFC_EXT_EN_HIGH |\
					PM_NFC_MBG_EN_HIGH |\
					PM_NFC_VDDLDO_OK_HIGH)

/*
 * pm8xxx_nfc_request - request a handle to access NFC device
 */
struct pm8xxx_nfc_device *pm8xxx_nfc_request(void);

/*
 * pm8xxx_nfc_config - configure NFC signals
 *
 * @nfcdev: the NFC device
 * @mask: signal mask to configure
 * @flags: control flags
 */
int pm8xxx_nfc_config(struct pm8xxx_nfc_device *nfcdev, u32 mask, u32 flags);

/*
 * pm8xxx_nfc_get_status - get NFC status
 *
 * @nfcdev: the NFC device
 * @mask: of status mask to read
 * @status: pointer to the status variable
 */
int pm8xxx_nfc_get_status(struct pm8xxx_nfc_device *nfcdev,
			  u32 mask, u32 *status);

/*
 * pm8xxx_nfc_free - free the NFC device
 */
void pm8xxx_nfc_free(struct pm8xxx_nfc_device *nfcdev);

#endif /* __PM8XXX_NFC_H__ */
