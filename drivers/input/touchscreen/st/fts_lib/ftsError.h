/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                  FTS error/info kernel log reporting                   *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */

#ifndef __FTS_ERROR_H
#define __FTS_ERROR_H


//FIRST LEVEL ERROR CODE
#define OK                             (int)(0x00000000)/* No ERROR*/
/*allocation of memory failed*/
#define ERROR_ALLOC                    (int)(0x80000001)
#define ERROR_I2C_R                    (int)(0x80000002)//i2c read failed
#define ERROR_I2C_W                    (int)(0x80000003)//i2c write failed
#define ERROR_I2C_WR                   (int)(0x80000004)//i2c write/read failed

//error during opening i2c device
#define ERROR_I2C_O                    (int)(0x80000005)
#define ERROR_OP_NOT_ALLOW             (int)(0x80000006)//operation not allowed

//timeout expired! exceed the max number of
//retries or the max waiting time
#define ERROR_TIMEOUT                  (int)(0x80000007)

//the file that i want to open is not found
#define ERROR_FILE_NOT_FOUND           (int)(0x80000008)
//error during parsing the file
#define ERROR_FILE_PARSE               (int)(0x80000009)
//error during reading the file
#define ERROR_FILE_READ                (int)(0x8000000A)
#define ERROR_LABEL_NOT_FOUND          (int)(0x8000000B)//label not found

//fw in the chip newer than the one in the memmh
#define ERROR_FW_NO_UPDATE             (int)(0x8000000C)
//flash status busy or unknown
#define ERROR_FLASH_UNKNOWN            (int)(0x8000000D)

//SECOND LEVEL ERROR CODE
//unable to disable the interrupt
#define ERROR_DISABLE_INTER            (int)(0x80000200)

//unable to activate the interrupt
#define ERROR_ENABLE_INTER             (int)(0x80000300)

#define ERROR_READ_B2                  (int)(0x80000400)//B2 command failed

//unable to read an offset from memory
#define ERROR_GET_OFFSET               (int)(0x80000500)

//unable to retrieve the data of a required frame
#define ERROR_GET_FRAME_DATA           (int)(0x80000600)

//FW answers with an event that has a
//different address respect the request done
#define ERROR_DIFF_COMP_TYPE           (int)(0x80000700)

//the signature of the compensation data is not A5
#define ERROR_WRONG_COMP_SIGN          (int)(0x80000800)
//the command Sense On failed
#define ERROR_SENSE_ON_FAIL            (int)(0x80000900)
//the command Sense Off failed
#define ERROR_SENSE_OFF_FAIL           (int)(0x80000A00)

//the command SYSTEM RESET failed
#define ERROR_SYSTEM_RESET_FAIL        (int)(0x80000B00)

//flash status not ready within a timeout
#define ERROR_FLASH_NOT_READY          (int)(0x80000C00)

//unable to retrieve fw_vers or the config_id
#define ERROR_FW_VER_READ              (int)(0x80000D00)

//unable to enable/disable the gesture
#define ERROR_GESTURE_ENABLE_FAIL      (int)(0x80000E00)

//unable to start to add custom gesture
#define ERROR_GESTURE_START_ADD        (int)(0x80000F00)

//unable to finish to add custom gesture
#define ERROR_GESTURE_FINISH_ADD       (int)(0x80001000)

//unable to add custom gesture data
#define ERROR_GESTURE_DATA_ADD         (int)(0x80001100)

//unable to remove custom gesture data
#define ERROR_GESTURE_REMOVE           (int)(0x80001200)

//unable to enable/disable a feature mode in the IC
#define ERROR_FEATURE_ENABLE_DISABLE   (int)(0x80001300)

//unable to set/read noise parameter in the IC
#define ERROR_NOISE_PARAMETERS         (int)(0x80001400)

//unable to write/rewrite/read lockdown code in the IC
#define ERROR_LOCKDOWN_CODE            (int)(0x80001500)

//THIRD LEVEL ERROR CODE
//unable to retrieve the force and/or sense length
#define ERROR_CH_LEN                   (int)(0x80010000)

//compensation data request failed
#define ERROR_REQU_COMP_DATA           (int)(0x80020000)

//unable to retrieve the compensation data header
#define ERROR_COMP_DATA_HEADER         (int)(0x80030000)

//unable to retrieve the global compensation data
#define ERROR_COMP_DATA_GLOBAL         (int)(0x80040000)

//unable to retrieve the compensation data for each node
#define ERROR_COMP_DATA_NODE           (int)(0x80050000)

//check of production limits or of fw answers failed
#define ERROR_TEST_CHECK_FAIL          (int)(0x80060000)
#define ERROR_MEMH_READ                (int)(0x80070000)//memh reading failed
#define ERROR_FLASH_BURN_FAILED        (int)(0x80080000)//flash burn failed
#define ERROR_MS_TUNING                (int)(0x80090000)//ms tuning failed
#define ERROR_SS_TUNING                (int)(0x800A0000)//ss tuning failed
//lp timer calibration failed
#define ERROR_LP_TIMER_TUNING          (int)(0x800B0000)
//save cx data to flash failed
#define ERROR_SAVE_CX_TUNING           (int)(0x800C0000)

//stop the poll of the FIFO if particular errors are found
#define ERROR_HANDLER_STOP_PROC        (int)(0x800D0000)
//unable to retrieve echo event
#define ERROR_CHECK_ECHO_FAIL          (int)(0x800E0000)

//FOURTH LEVEL ERROR CODE
//production data test failed
#define ERROR_PROD_TEST_DATA           (int)(0x81000000)

//complete flash procedure failed
#define ERROR_FLASH_PROCEDURE          (int)(0x82000000)
//production ito test failed
#define ERROR_PROD_TEST_ITO            (int)(0x83000000)

//production initialization test failed
#define ERROR_PROD_TEST_INITIALIZATION (int)(0x84000000)

//mismatch of the MS or SS tuning_version
#define ERROR_GET_INIT_STATUS          (int)(0x85000000)


void logError(int force, const char *msg, ...);
int isI2cError(int error);
int dumpErrorInfo(void);
int errorHandler(u8 *event, int size);

#endif
