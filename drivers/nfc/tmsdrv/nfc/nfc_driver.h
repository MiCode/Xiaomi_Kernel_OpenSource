/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#ifndef _TMS_NFC_THN31_H_
#define _TMS_NFC_THN31_H_

/*********** PART0: Head files ***********/
#include <linux/poll.h>
#include <linux/timekeeping.h>
#include "nfc_common.h"
/*********** PART1: Define Area ***********/
#define NFC_DEVICE               "tms,nfc"
#define MAX_BUFFER_SIZE           (4096)
#define NFC_CMD_RSP_TIMEOUT_MS    (2000)
#define NFC_MAGIC                 (0xE9)
#define NFC_SET_STATE             _IOW(NFC_MAGIC, 0x01, long)
#define NFC_SET_ESE_STATE         _IOW(NFC_MAGIC, 0x02, long)
#define NFC_GET_ESE_STATE         _IOR(NFC_MAGIC, 0x03, long)
#define NFC_IRQ_STATE             _IOW(NFC_MAGIC, 0x0C, long)

#define T1_HEAD 0X5A
#define I2C_ELAPSE_TIMEOUT (1500 * 1000 * 1000L)
#define MAX_I2C_WAKEUP_TIME 3
#define I2C_WAKEUP_SLEEP_TIME1 5000
#define I2C_WAKEUP_SLEEP_TIME2 5100

/*********** PART2: Struct Area ***********/
enum nfc_ioctl_request_table {
    NFCC_POWER_OFF    = 0,  /* NFCC power off with ven low */
    NFCC_POWER_ON     = 1,  /* NFCC power on with ven high */
    NFCC_FW_DWNLD_OFF = 6,  /* NFCC firmware download gpio low */
    NFCC_FW_DWNLD_ON  = 4,  /* NFCC firmware download gpio high */
    NFCC_HARD_RESET   = 5, /* NFCC hard reset */
    /*TODO : After hal download modified to romove */
    NFC_DLD_PWR_VEN_ON  = 10,
    NFC_DLD_PWR_VEN_OFF = 11,
    NFC_DLD_PWR_DL_ON   = 12,
    NFC_DLD_PWR_DL_OFF  = 13,
    NFC_DLD_FLUSH       = 14,
};

/*********** PART3: Function or variables for other files ***********/
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
int nfc_device_probe(struct i2c_client *client, const struct i2c_device_id *id);
#else
int nfc_device_probe(struct i2c_client *client);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,1,0)
int nfc_device_remove(struct i2c_client *client);
#else
void nfc_device_remove(struct i2c_client *client);
#endif
int nfc_device_suspend(struct device *device);
int nfc_device_resume(struct device *device);
#endif /* _TMS_NFC_THN31_H_ */
