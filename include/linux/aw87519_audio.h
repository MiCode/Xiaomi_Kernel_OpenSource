/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 Awinic Inc.
 */

#ifndef __AW87519_H__
#define __AW87519_H__

/******************************************************
 *
 *Load config function
 *This driver will use load firmware if AW87519_BIN_CONFIG be defined
 *****************************************************/
#define AWINIC_CFG_UPDATE_DELAY
#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define REG_CHIPID            0x00
#define REG_SYSCTRL           0x01
#define REG_BATSAFE           0x02
#define REG_BSTOVR            0x03
#define REG_BSTVPR            0x04
#define REG_PAGR              0x05
#define REG_PAGC3OPR          0x06
#define REG_PAGC3PR           0x07
#define REG_PAGC2OPR          0x08
#define REG_PAGC2PR           0x09
#define REG_PAGC1PR           0x0A

#define AW87519_CHIPID      0x59
#define AW87519_REG_MAX     11
#define AW87519_VAL 0660


/*******************************************************************************
 * aw87519 functions
 ******************************************************************************/
unsigned char aw87519_left_audio_receiver(void);
unsigned char aw87519_amp_lch_on(void);
unsigned char aw87519_amp_lch_off(void);
unsigned char aw87519_right_audio_receiver(void);
unsigned char aw87519_amp_rch_on(void);
unsigned char aw87519_amp_rch_off(void);
#endif
