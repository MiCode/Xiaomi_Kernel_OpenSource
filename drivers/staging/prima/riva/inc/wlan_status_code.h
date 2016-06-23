/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================
 *
 *  @file:     wlan_status_code.h
 *
 *  @brief:    Common header file containing all the status codes
 *             All status codes have been consolidated into one enum
 *
 *  @author:   Kumar Anand
 *
 *=========================================================================*/

#ifndef __WLAN_STATUS_CODE_H__
#define __WLAN_STATUS_CODE_H__

/*-------------------------------------------------------------------------
  Include Files
-------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/* This is to force compiler to use the maximum of an int ( 4 bytes ) */
#define WLAN_STATUS_MAX_ENUM_SIZE    0x7FFFFFFF

/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/

typedef enum
{
   /* PAL Request succeeded!*/
   PAL_STATUS_SUCCESS = 0,

   /* HAL Request succeeded!*/
   eHAL_STATUS_SUCCESS = 0,

   /* Request failed because there of an invalid request.  This is
      typically the result of invalid parameters on the request*/
   PAL_STATUS_INVAL,

   /* Request refused because a request is already in place and
      another cannot be handled currently */
   PAL_STATUS_ALREADY,

   /* Request failed because of an empty condition */
   PAL_STATUS_EMPTY,

   /* Request failed for some unknown reason. */
   PAL_STATUS_FAILURE,

   /* HAL general failure */
   eHAL_STATUS_FAILURE,

   /* Invalid Param*/
   eHAL_STATUS_INVALID_PARAMETER,

   /* Invalid Station Index*/
   eHAL_STATUS_INVALID_STAIDX,

   /* DPU descriptor table full*/
   eHAL_STATUS_DPU_DESCRIPTOR_TABLE_FULL,

   /* No interrupts */
   eHAL_STATUS_NO_INTERRUPTS,

   /* Interrupt present */
   eHAL_STATUS_INTERRUPT_PRESENT,

   /* Stable Table is full */
   eHAL_STATUS_STA_TABLE_FULL,

   /* Duplicate Station found */
   eHAL_STATUS_DUPLICATE_STA,

   /* BSSID is invalid */
   eHAL_STATUS_BSSID_INVALID,

   /* STA is invalid */
   eHAL_STATUS_STA_INVALID,

   /* BSSID is is duplicate */
   eHAL_STATUS_DUPLICATE_BSSID,

   /* BSS Idx is invalid */
   eHAL_STATUS_INVALID_BSSIDX,

   /* BSSID Table is full */
   eHAL_STATUS_BSSID_TABLE_FULL,

   /* Invalid DPU signature*/
   eHAL_STATUS_INVALID_SIGNATURE,

   /* Invalid key Id */
   eHAL_STATUS_INVALID_KEYID,

   /* Already on requested channel */
   eHAL_STATUS_SET_CHAN_ALREADY_ON_REQUESTED_CHAN,

   /* UMA descriptor table is full */
   eHAL_STATUS_UMA_DESCRIPTOR_TABLE_FULL,

   /* MIC Key table is full */
   eHAL_STATUS_DPU_MICKEY_TABLE_FULL,

   /* A-MPDU/BA related Error codes */
   eHAL_STATUS_BA_RX_BUFFERS_FULL,
   eHAL_STATUS_BA_RX_MAX_SESSIONS_REACHED,
   eHAL_STATUS_BA_RX_INVALID_SESSION_ID,

   eHAL_STATUS_TIMER_START_FAILED,
   eHAL_STATUS_TIMER_STOP_FAILED,
   eHAL_STATUS_FAILED_ALLOC,

   /* Scan failure codes */
   eHAL_STATUS_NOTIFY_BSS_FAIL,

   /* Self STA not deleted as reference count is not zero */
   eHAL_STATUS_DEL_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO,

   /* Self STA not added as entry already exists*/
   eHAL_STATUS_ADD_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO,

   /* Message from SLM has failure status */
   eHAL_STATUS_FW_SEND_MSG_FAILED,
   
   /* BSS disconnect status : beacon miss */
   eHAL_STATUS_BSS_DISCONN_BEACON_MISS,
   /* BSS disconnect status : deauth */
   eHAL_STATUS_BSS_DISCONN_DEAUTH,
   /* BSS disconnect status : disassoc */
   eHAL_STATUS_BSS_DISCONN_DISASSOC,
   
   /* Data abort happened in PHY sw */
   eHAL_STATUS_PHY_DATA_ABORT,

   /* Invalid NV field  */
   eHAL_STATUS_PHY_INVALID_NV_FIELD,

   /* WLAN boot test failed */
   eHAL_STATUS_WLAN_BOOT_TEST_FAILURE,

   /* Max status value */
   eHAL_STATUS_MAX_VALUE = WLAN_STATUS_MAX_ENUM_SIZE

} palStatus, eHalStatus;

/* Helper Macros */
#define PAL_IS_STATUS_SUCCESS(status) (PAL_STATUS_SUCCESS  == (status))
#define HAL_STATUS_SUCCESS( status )  (eHAL_STATUS_SUCCESS == (status))

#endif //__WLAN_STATUS_CODE_H__
