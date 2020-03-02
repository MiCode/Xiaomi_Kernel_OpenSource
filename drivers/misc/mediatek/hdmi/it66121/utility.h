/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
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

#ifndef _Utility_h_
#define _Utility_h_
#include "itx_typedef.h"
#include "mcu.h"

/* #define MS_TimeOut(x) (((x)+LOOPMS-1)/LOOPMS) */
/* #define MS_TimeOut(x) ((x)+1)//(x/20)+1 */
/*#define VSTATE_MISS_SYNC_COUNT MS_TimeOut(2000)*/
/*#define VSATE_CONFIRM_SCDT_COUNT MS_TimeOut(150)*/
/*#define AUDIO_READY_TIMEOUT MS_TimeOut(200)*/
/*#define AUDIO_STABLE_TIMEOUT MS_TimeOut(1000)*/
/*#define VSATE_CONFIRM_SCDT_COUNT MS_TimeOut(0)*/
/*#define AUDIO_READY_TIMEOUT MS_TimeOut(10)*/
/*#define AUDIO_STABLE_TIMEOUT MS_TimeOut(100)*/

/*#define MUTE_RESUMING_TIMEOUT MS_TimeOut(2500)*/
/*#define HDCP_WAITING_TIMEOUT MS_TimeOut(3000)*/

/*#define FORCE_SWRESET_TIMEOUT  MS_TimeOut(32766)*/

#define VSTATE_CDR_DISCARD_TIME MS_TimeOut(6000)

#define VSTATE_MISS_SYNC_COUNT MS_TimeOut(5000)
#define VSTATE_SWRESET_TIMEOUT_COUNT MS_TimeOut(50)
#define VSATE_CONFIRM_SCDT_COUNT MS_TimeOut(20)
#define AUDIO_READY_TIMEOUT MS_TimeOut(20)
#define AUDIO_STABLE_TIMEOUT MS_TimeOut(20) /* MS_TimeOut(1000) */

#define MUTE_RESUMING_TIMEOUT MS_TimeOut(2500)
#define HDCP_WAITING_TIMEOUT MS_TimeOut(3000)

#define FORCE_SWRESET_TIMEOUT MS_TimeOut(15000)

#define TX_UNPLUG_TIMEOUT MS_TimeOut(300)
#define TX_WAITVIDSTBLE_TIMEOUT MS_TimeOut(100)

#define TX_HDCP_TIMEOUT MS_TimeOut(6000)

#define CAT_HDMI_PORTA 0
#define CAT_HDMI_PORTB 1
/* ///////////////////////////////////////////////////// */
/* Global variable */
/* ////////////////////////////////////////////////////// */
/* alex 070327 */
/* for chroma2229 audio error */

/* ///////////////////////////////////////////////////////// */
/* Function Prototype */
/* ///////////////////////////////////////////////////////// */

/* void SetTxLED(unsigned char device,bool status); */

void HoldSystem(void);
void delay1ms(unsigned short ms);
void HoldSystem(void);

void ConfigfHdmiVendorSpecificInfoFrame(unsigned char _3D_Stru);
void SetOutputColorDepthPhase(unsigned char ColorDepth, unsigned char bPhase);

#endif /* _Utility_h_ */
