/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TYPEC_PD_H
#define _TYPEC_PD_H

#define SUPPORT_PD 0

#if SUPPORT_PD

/**************************************************************************/

#define PD_DVT 1

/*W1C RCV_MSG_INTR clears RX buffer, choose method 1 or 2
 * method 1: read RX buffer and W1C RCV_MSG_INTR in ISR
 * mtthod 2: leave RCV_MSG_INTR untouched, read RX buffer and W1C it in main loop
 */
#define PD_SW_WORKAROUND1_1 0
#define PD_SW_WORKAROUND1_2 1

/*use timer0 for pd_task main loop*/
/*this should be enabled when the scale of jiffies is not small enough or not accurate*/
#define PD_SW_WORKAROUND2 1

/*use timer1 to replace system jiffies*/
/*this should be enabled when the scale of jiffies is not small enough or not accurate*/
#define PD_SW_WORKAROUND3 1

/*to workaround DTB OVP issue. [COHEC00012694] [PD] In PR_SWAP process, PS_RDY from UFP is NOT acked by GCRC*/
#define PD_SW_WORKAROUND4 0

/**************************************************************************/

#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_COMMON_RUNTIME

#include "pd_reg.h"
#include "usb_pd.h"

/**************************************************************************/

/*interrupt classification - by type*/

/*TX/TX(Auto SR)/RX interrupts -- SUCCESS*/
#define PD_TX_MSG_SUCCESS (PD_TX_DONE_INTR)
#define PD_TX_AUTO_SR_SUCCESS (PD_TX_AUTO_SR_DONE_INTR)
#define PD_TX_HR_SUCCESS (PD_HR_TRANS_DONE_INTR)
#define PD_RX_MSG_SUCCESS (PD_RX_RCV_MSG_INTR)
#define PD_RX_HR_SUCCESS (PD_HR_RCV_DONE_INTR)

/*TX/TX(Auto SR)/RX interrupts -- FAIL*/
#define PD_TX_MSG_FAIL (PD_TX_RETRY_ERR_INTR | \
	PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR | PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR)
#define PD_TX_AUTO_SR_FAIL (PD_TX_AUTO_SR_RETRY_ERR_INTR | \
	PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR | PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR)
#define PD_TX_HR_FAIL (PD_HR_TRANS_CPL_TIMEOUT_INTR | PD_HR_TRANS_FAIL_INTR)

/*TX/TX(Auto SR)/RX interrupts -- INFO ONLY*/
#define PD_TX_MSG_INFO (PD_TX_DIS_BUS_REIDLE_INTR | PD_TX_CRC_RCV_TIMEOUT_INTR)
#define PD_TX_AUTO_SR_FAIL_INFO (PD_TX_MSG_INFO)
#define PD_RX_MSG_FAIL_INFO (PD_RX_LENGTH_MIS_INTR | PD_RX_DUPLICATE_INTR | PD_RX_TRANS_GCRC_FAIL_INTR)

/**************************************************************************/

/*interrupt classification - by register*/

#define PD_TX_SUCCESS0 (PD_TX_MSG_SUCCESS | PD_TX_AUTO_SR_SUCCESS)
#define PD_TX_FAIL0 (PD_TX_MSG_FAIL | PD_TX_AUTO_SR_FAIL)
#define PD_TX_INFO0 (PD_TX_MSG_INFO)
#define PD_RX_SUCCESS0 (PD_RX_MSG_SUCCESS)
#define PD_RX_INFO0 (PD_RX_MSG_FAIL_INFO)

#define PD_TX_SUCCESS1 (PD_TX_HR_SUCCESS)
#define PD_TX_FAIL1 (PD_TX_HR_FAIL)
#define PD_RX_SUCCESS1 (PD_RX_HR_SUCCESS)

#define PD_TX_EVENTS0_LISTEN (PD_TX_SUCCESS0 | PD_TX_FAIL0)
#define PD_TX_EVENTS0 (PD_TX_EVENTS0_LISTEN | PD_TX_INFO0)
#define PD_RX_EVENTS0_LISTEN (PD_RX_SUCCESS0)
#define PD_EVENTS0 (PD_RX_EVENTS0_LISTEN | PD_RX_INFO0)

#define PD_TX_EVENTS1_LISTEN (PD_TX_SUCCESS1 | PD_TX_FAIL1)
#define PD_TX_EVENTS1 (PD_TX_EVENTS1_LISTEN)
#define PD_RX_EVENTS1_LISTEN (PD_RX_SUCCESS1)
#define PD_EVENTS1 (PD_RX_EVENTS1_LISTEN | PD_TIMER0_TIMEOUT_INTR)

/**************************************************************************/

#define PD_INTR_EN_0_MSK (PD_TX_SUCCESS0 | PD_TX_FAIL0 | PD_TX_INFO0 | \
	PD_RX_SUCCESS0 | PD_RX_INFO0)

#define PD_INTR_EN_1_MSK (PD_TX_SUCCESS1 | PD_TX_FAIL1 | PD_RX_SUCCESS1 | \
	PD_TIMER0_TIMEOUT_INTR)

/*interrupts serviced by PD task*/
#define PD_INTR_IS0_LISTEN (TYPE_C_CC_ENT_DISABLE_INTR | \
	TYPE_C_CC_ENT_UNATTACH_SNK_INTR | TYPE_C_CC_ENT_ATTACH_SNK_INTR | \
	TYPE_C_CC_ENT_UNATTACH_SRC_INTR  | TYPE_C_CC_ENT_ATTACH_SRC_INTR)

/**************************************************************************/

/* Time to wait for TCPC to complete transmit */
#define PD_TX_TIMEOUT (100*1)

/**************************************************************************/

/* board.h */

/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  (20*1) /*30*1*/
#if PD_SW_WORKAROUND4
#define PD_POWER_SUPPLY_TURN_ON_DELAY1  (250*1)
#endif
#define PD_POWER_SUPPLY_TURN_OFF_DELAY (250*1)
#define PD_VCONN_SWAP_DELAY (10*1)

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     5000

/**************************************************************************/

#define PD_REF_CK (12 * 1000000)
#define PD_REF_CK_MS (PD_REF_CK / 1000)

/**************************************************************************/

enum vdm_states {
	VDM_STATE_ERR_BUSY = -3,
	VDM_STATE_ERR_SEND = -2,
	VDM_STATE_ERR_TMOUT = -1,
	VDM_STATE_DONE = 0,
	/* Anything >0 represents an active state */
	VDM_STATE_READY = 1,
	VDM_STATE_BUSY = 2,
	VDM_STATE_WAIT_RSP_BUSY = 3,
};

/**************************************************************************/

#endif
#endif
