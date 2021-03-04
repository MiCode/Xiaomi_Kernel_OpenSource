/*
 * Copyright (C) 2016 ST Microelectronics S.A.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright (C) 2010 Stollmann E+V GmbH
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#define ST21NFC_MAGIC 0xEA

#define ST21NFC_NAME "st21nfc"
//#define USE_NODE      1
/*
 * ST21NFC power control via ioctl
 * ST21NFC_GET_WAKEUP :  poll gpio-level for Wakeup pin
 */
#define ST21NFC_GET_WAKEUP _IO(ST21NFC_MAGIC, 0x01)
#define ST21NFC_PULSE_RESET _IO(ST21NFC_MAGIC, 0x02)
#define ST21NFC_SET_POLARITY_RISING _IO(ST21NFC_MAGIC, 0x03)
#define ST21NFC_SET_POLARITY_HIGH _IO(ST21NFC_MAGIC, 0x05)
#define ST21NFC_GET_POLARITY _IO(ST21NFC_MAGIC, 0x07)
#define ST21NFC_RECOVERY _IO(ST21NFC_MAGIC, 0x08)
#define ST21NFC_USE_ESE _IOW(ST21NFC_MAGIC, 0x09, unsigned int)
#define ST21NFC_SET_ESEPWR _IOW(ST21NFC_MAGIC, 0x0A, unsigned int)
#define ST21NFC_GET_ESEPWR _IOR(ST21NFC_MAGIC, 0x0B, unsigned int)

// Keep compatibility with older user applications.
#define ST21NFC_LEGACY_GET_WAKEUP _IOR(ST21NFC_MAGIC, 0x01, unsigned int)
#define ST21NFC_LEGACY_PULSE_RESET _IOR(ST21NFC_MAGIC, 0x02, unsigned int)
#define ST21NFC_LEGACY_SET_POLARITY_RISING \
  _IOR(ST21NFC_MAGIC, 0x03, unsigned int)
#define ST21NFC_LEGACY_SET_POLARITY_HIGH _IOR(ST21NFC_MAGIC, 0x05, unsigned int)
#define ST21NFC_LEGACY_GET_POLARITY _IOR(ST21NFC_MAGIC, 0x07, unsigned int)
#define ST21NFC_LEGACY_RECOVERY _IOR(ST21NFC_MAGIC, 0x08, unsigned int)

#define ST54SPI_CB_RESET_END 0
#define ST54SPI_CB_RESET_START 1
#define ST54SPI_CB_ESE_NOT_USED 2
#define ST54SPI_CB_ESE_USED 3
void st21nfc_register_st54spi_cb(void (*cb)(int, void *), void *data);
void st21nfc_unregister_st54spi_cb(void);
