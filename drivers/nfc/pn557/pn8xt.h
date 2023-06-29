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
#ifndef _NXP_NFC_PN8XT_H_
#define _NXP_NFC_PN8XT_H_

/*
 * NFC power control via ioctl
 * PN8XT_SET_PWR(0): power off
 * PN8XT_SET_PWR(1): power on
 * PN8XT_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN8XT_SET_PWR      _IOW(NXP_NFC_MAGIC, 0x01, long)
/*
 * SPI Request NFCC to enable ESE power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define PN8XT_SET_SPI_PWR  _IOW(NXP_NFC_MAGIC, 0x02, long)

/* SPI or DWP can call this ioctl to get the current
 * power state of ESE
 *
*/
#define PN8XT_GET_PWR_STATUS    _IOR(NXP_NFC_MAGIC, 0x03, long)

/* DWP side this ioctl will be called
 * level 1 = Wired access is enabled/ongoing
 * level 0 = Wired access is disabled/stopped
*/
#define PN8XT_SET_WIRED_ACCESS _IOW(NXP_NFC_MAGIC, 0x04, long)

/*
  NFC Init will call the ioctl to register the PID with the i2c driver
*/
#define PN8XT_SET_NFC_SERVICE_PID _IOW(NXP_NFC_MAGIC, 0x05, long)

/*
  NFC and SPI will call the ioctl to get the i2c/spi bus access
*/
#define PN8XT_GET_ESE_ACCESS _IOW(NXP_NFC_MAGIC, 0x06, long)
/*
  NFC and SPI will call the ioctl to update the power scheme
*/
#define PN8XT_SET_POWER_SCM _IOW(NXP_NFC_MAGIC, 0x07, long)

/*
  NFC will call the ioctl to release the svdd protection
*/
#define PN8XT_REL_SVDD_WAIT _IOW(NXP_NFC_MAGIC, 0x08, long)

/* SPI or DWP can call this ioctl to get the current
 * power state of ESE
 *
*/
#define PN8XT_SET_DN_STATUS    _IOW(NXP_NFC_MAGIC, 0x09, long)
/*
  NFC will call the ioctl to release the dwp on/off protection
*/

#define PN8XT_REL_DWP_WAIT _IOW(NXP_NFC_MAGIC, 0x0A, long)

/*
  NFC will call the ioctl to start Secure Timer
*/

#define PN8XT_SECURE_TIMER_SESSION _IOW(NXP_NFC_MAGIC, 0x0B, long)

#define MAX_ESE_ACCESS_TIME_OUT_MS 200

typedef enum pn8xt_access_state {
    ST_INVALID              = 0x0000,
    ST_IDLE                 = 0x0100,   /*ESE is free to use */
    ST_WIRED                = 0x0200,   /*ESE is being accessed by DWP (NFCC)*/
    ST_SPI                  = 0x0400,   /*ESE is being accessed by SPI */
    ST_DN                   = 0x0800,   /*NFCC fw download is in progress */
    ST_SPI_PRIO             = 0x1000,   /*Start of ESE access by SPI on priority*/
    ST_SPI_PRIO_END         = 0x2000,   /*End of ESE access by SPI on priority*/
    ST_SPI_END              = 0x4000,
    ST_JCP_DN               = 0x8000,   /*JCOP downlad in progress */
    ST_SECURE_MODE          = 0x100000, /*secure mode state*/
    ST_SPI_SVDD_SY_START    = 0x0001,   /*ESE_VDD Low req by SPI*/
    ST_SPI_SVDD_SY_END      = 0x0002,   /*ESE_VDD is Low by SPI*/
    ST_DWP_SVDD_SY_START    = 0x0004,   /*ESE_VDD  Low req by Nfc*/
    ST_DWP_SVDD_SY_END      = 0x0008,   /*ESE_VDD is Low by Nfc*/
    ST_SPI_FAILED           = 0x0010    /*SPI open/close failed*/
}pn8xt_access_st_t;

typedef enum pn8xt_pwr_scheme {
    PN80T_LEGACY_PWR_SCM  = 0x02,
    PN80T_EXT_PMU_SCM,
}pn8xt_pwr_scm_t;

typedef enum pn8xt_jcop_dwnld_state {
    JCP_DN_IDLE = ST_JCP_DN,        /* jcop dwnld is ongoing*/
    JCP_DN_INIT=0x8010,             /* jcop dwonload init state*/
    JCP_DN_START=0x8020,            /* download started */
    JCP_SPI_DN_COMP=0x8040,         /* jcop download complete in spi interface*/
    JCP_DWP_DN_COMP=0x8080,         /* jcop download complete */
} pn8xt_jcop_dwnld_state_t;

long pn8xt_nfc_ese_ioctl(struct nfc_dev *nfc_dev,  unsigned int cmd, unsigned long arg);
long pn8xt_nfc_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg);
int pn8xt_nfc_probe(struct nfc_dev *nfc_dev);
int pn8xt_nfc_remove(struct nfc_dev *nfc_dev);
#endif //_NXP_NFC_PN8XT_H_