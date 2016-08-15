/***********************************************************************************/
/*  Copyright (c) 2002-2010, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/

#include "si_mhl_defs.h"
#include "sii_reg_access.h"
#include "si_drv_mhl_tx.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_9244_regs.h"
#include "sii_9244_api.h"
#include "si_app_devcap.h"

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mhl_api.h>
#include <linux/delay.h>

#define SILICON_IMAGE_ADOPTER_ID 322
#define TRANSCODER_DEVICE_ID 0x9244


//
// Software power states are a little bit different than the hardware states but
// a close resemblance exists.
//
// D3 matches well with hardware state. In this state we receive RGND interrupts
// to initiate wake up pulse and device discovery
//
// Chip wakes up in D2 mode and interrupts MCU for RGND. Firmware changes the 9244
// into D0 mode and sets its own operation mode as POWER_STATE_D0_NO_MHL because
// MHL connection has not yet completed.
//
// For all practical reasons, firmware knows only two states of hardware - D0 and D3.
//
// We move from POWER_STATE_D0_NO_MHL to POWER_STATE_D0_MHL only when MHL connection
// is established.
/*
//
//                             S T A T E     T R A N S I T I O N S
//
//
//                    POWER_STATE_D3                      POWER_STATE_D0_NO_MHL
//                   /--------------\                        /------------\
//                  /                \                      /     D0       \
//                 /                  \                \   /                \
//                /   DDDDDD  333333   \     RGND       \ /   NN  N  OOO     \
//                |   D     D     33   |-----------------|    N N N O   O     |
//                |   D     D  3333    |      IRQ       /|    N  NN  OOO      |
//                \   D     D      33  /               /  \                  /
//                 \  DDDDDD  333333  /                    \   CONNECTION   /
//                  \                /\                     /\             /
//                   \--------------/  \  TIMEOUT/         /  -------------
//                         /|\          \-------/---------/        ||
//                        / | \            500ms\                  ||
//                          |                     \                ||
//                          |  RSEN_LOW                            || MHL_EST
//                           \ (STATUS)                            ||  (IRQ)
//                            \                                    ||
//                             \      /------------\              //
//                              \    /              \            //
//                               \  /                \          //
//                                \/                  \ /      //
//                                 |    CONNECTED     |/======//
//                                 |                  |\======/
//                                 \   (OPERATIONAL)  / \
//                                  \                /
//                                   \              /
//                                    \-----------/
//                                   POWER_STATE_D0_MHL
//
//
//
 */
#define	POWER_STATE_D3				3
#define	POWER_STATE_D0_NO_MHL		2
#define	POWER_STATE_D0_MHL			0
#define	POWER_STATE_FIRST_INIT		0xFF

//#define TSRC_RSEN_DEGLITCH_INCLUDED_IN_TSRC_RSEN_CHK

//
// To remember the current power state.
//
uint8_t	fwPowerState = POWER_STATE_FIRST_INIT;

//
// This flag is set to true as soon as a INT1 RSEN CHANGE interrupt arrives and
// a deglitch timer is started.
//
// We will not get any further interrupt so the RSEN LOW status needs to be polled
// until this timer expires.
//
#ifndef TSRC_RSEN_DEGLITCH_INCLUDED_IN_TSRC_RSEN_CHK
static	bool_t	deglitchingRsenNow = false;
#endif
//
// To serialize the RCP commands posted to the CBUS engine, this flag
// is maintained by the function SiiMhlTxDrvSendCbusCommand()
//
static	bool_t		mscCmdInProgress;
//
// Preserve Downstream HPD status
//
static	uint8_t	dsHpdStatus = 0;
static uint8_t linkMode = 0;
static uint8_t contentOn = 0;

#define	I2C_READ_MODIFY_WRITE(saddr, offset, mask)	I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) | (mask));
#define ReadModifyWriteByteCBUS(offset, andMask, orMask)  WriteByteCBUS(offset, (ReadByteCBUS(offset)&andMask) | orMask)



#define	SET_BIT(saddr, offset, bitnumber)		I2C_READ_MODIFY_WRITE(saddr, offset, (1<<bitnumber))
#define	CLR_BIT(saddr, offset, bitnumber)		I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) & ~(1<<bitnumber))
//
// 90[0] = Enable / Disable MHL Discovery on MHL link
//
#define	DISABLE_DISCOVERY				CLR_BIT(PAGE_0_0X72, 0x90, 0);
#define	ENABLE_DISCOVERY				SET_BIT(PAGE_0_0X72, 0x90, 0);

#define STROBE_POWER_ON                    CLR_BIT(PAGE_0_0X72, 0x90, 1);
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(only if necessary)
//
#define	INTR_4_DESIRED_MASK				(BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_INTR_4_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x78, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x78, 0x00)

//	Look for interrupts on INTR_2 (Register 0x72)
//		7 = bcap done			(ignore)
//		6 = parity error		(ignore)
//		5 = ENC_EN changed		(ignore)
//		4 = no premable			(ignore)
//		3 = ACR CTS changed		(ignore)
//		2 = ACR Pkt Ovrwrt		(ignore)
//		1 = TCLK_STBL changed	(interested)
//		0 = Vsync				(ignore)
#define	INTR_2_DESIRED_MASK				(BIT1)
#define	UNMASK_INTR_2_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x76, INTR_2_DESIRED_MASK)
#define	MASK_INTR_2_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x76, 0x00)

//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)

#define	INTR_1_DESIRED_MASK				(BIT5 | BIT6)
#define	UNMASK_INTR_1_INTERRUPTS		I2C_WriteByte(PAGE_0_0X72, 0x75, INTR_1_DESIRED_MASK)
#define	MASK_INTR_1_INTERRUPTS			I2C_WriteByte(PAGE_0_0X72, 0x75, 0x00)

//	Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
#define	INTR_CBUS1_DESIRED_MASK			(BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_CBUS1_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x09, INTR_CBUS1_DESIRED_MASK)
#define	MASK_CBUS1_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x09, 0x00)

#define	INTR_CBUS2_DESIRED_MASK			(BIT0 | BIT2 | BIT3)
#define	UNMASK_CBUS2_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x1F, INTR_CBUS2_DESIRED_MASK)
#define	MASK_CBUS2_INTERRUPTS			I2C_WriteByte(PAGE_CBUS_0XC8, 0x1F, 0x00)

#define I2C_INACCESSIBLE -1
#define I2C_ACCESSIBLE 1

//
// Local scope functions.
//
static	int 	Int4Isr(void);
static	void	MhlCbusIsr(void);


static	void	Int1RsenIsr(uint8_t rsen);
static	void 	DeglitchRsenLow(uint8_t rsen);


static	void	CbusReset(void);
static	void	SwitchToD0(void);
static	void	SwitchToD3(void);
static	void	WriteInitialRegisterValues(void);
static	void	InitCBusRegs(void);
static	void	ForceUsbIdSwitchOpen(void);
static	void	ReleaseUsbIdSwitchOpen(void);
static	void	MhlTxDrvProcessConnection(void);
static	void	MhlTxDrvProcessDisconnection(void);
static	void	ApplyDdcAbortSafety(void);

#define	APPLY_PLL_RECOVERY

#ifdef APPLY_PLL_RECOVERY
static  void    SiiMhlTxDrvRecovery(void);
#endif


static struct hrtimer hr_timer_RSEN_CHK;
static struct hrtimer hr_timer_RSEN_DEGLITCH;


static ktime_t hr_timer_RSEN_CHK_ktime;
static ktime_t hr_timer_RSEN_DEGLITCH_ktime;


static bool	timer_RSEN_CHK_Expired = false;
static bool	timer_RSEN_DEGLITCH_Expired = false;


unsigned long delay_in_ms;
#define MS_TO_NS(x)	(x * 1E6L)

bool mhl_is_connected(void)
{
	printk("%s: %d\n", __func__, contentOn);
	return (bool)contentOn;
}

enum hrtimer_restart timer_RSEN_CHK_callback(struct hrtimer *timer)
{
	timer_RSEN_CHK_Expired = true;
	TX_DEBUG_PRINT(("Drv: timer_RSEN_CHK_Expired now!!!!!!\n"));
	return HRTIMER_NORESTART;
}

enum hrtimer_restart timer_RSEN_DEGLITCH_callback(struct hrtimer *timer)
{
	timer_RSEN_DEGLITCH_Expired = true;
	TX_DEBUG_PRINT(("Drv: timer_RSEN_DEGLITCH_Expired now!!!!!!\n"));
	return HRTIMER_NORESTART;
}



////////////////////////////////////////////////////////////////////
//
// E X T E R N A L L Y    E X P O S E D   A P I    F U N C T I O N S
//
////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxChipInitialize
//
// Chip specific initialization.
// This function is for SiI 9244 Initialization: HW Reset, Interrupt enable.
//
//
//////////////////////////////////////////////////////////////////////////////

bool_t SiiMhlTxChipInitialize(void)
{
	uint8_t idh = 0;

	TX_DEBUG_PRINT(("Drv: SiiMhlTxChipInitialize: %02X44\n", (int)I2C_ReadByte(PAGE_0_0X72, 0x03)));

	idh = I2C_ReadByte(PAGE_0_0X72, 0x03);
	if (idh != 0x92) {
		TX_DEBUG_PRINT (("MhlTx: HW issue. Do not init MHL\n"));
		return false;
	}

	//
	// Setup our own timer for now. 50ms.
	//


	hrtimer_init(&hr_timer_RSEN_CHK, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer_RSEN_CHK.function = &timer_RSEN_CHK_callback;

	hrtimer_init(&hr_timer_RSEN_DEGLITCH, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer_RSEN_DEGLITCH.function = &timer_RSEN_DEGLITCH_callback;

	WriteInitialRegisterValues();

	I2C_WriteByte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK);

	UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;


	// CBUS interrupts are unmasked after performing the reset.
	// UNMASK_CBUS1_INTERRUPTS;
	// UNMASK_CBUS2_INTERRUPTS;

	//
	// Allow regular operation - i.e. pinAllowD3 is high so we do enter
	// D3 first time. Later on, SiIMon shall control this GPIO.
	//
	//pinAllowD3 = 1;

	SwitchToD3();

	return true;
}




///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDeviceIsr
//
// This function must be called from a master interrupt handler or any polling
// loop in the host software. SiiMhlTxGetEvents will not look at these
// events assuming firmware is operating in interrupt driven mode. MhlTx component
// performs a check of all its internal status registers to see if a hardware event
// such as connection or disconnection has happened or an RCP message has been
// received from the connected device.
// MhlTx code will ensure concurrency by asking the host software and hardware to
// disable interrupts and restore when completed. Device interrupts are cleared by
// the MhlTx component before returning back to the caller. Any handling of
// programmable interrupt controller logic if present in the host will have to
// be done by the caller after this function returns back.

// This function has no parameters and returns nothing.
//
// This is the master interrupt handler for 9244. It calls sub handlers
// of interest. Still couple of status would be required to be picked up
// in the monitoring routine (Sii9244TimerIsr)
//
// To react in least amount of time hook up this ISR to processor's
// interrupt mechanism.
//
// Just in case environment does not provide this, set a flag so we
// call this from our monitor (Sii9244TimerIsr) in periodic fashion.
//
// Device Interrupts we would look at
//		RGND		= to wake up from D3
//		MHL_EST 	= connection establishment
//		CBUS_LOCKOUT= Service USB switch
//		RSEN_LOW	= Disconnection deglitcher
//		CBUS 		= responder to peer messages
//					  Especially for DCAP etc time based events
//
void 	SiiMhlTxDeviceIsr(void)
{
	//
	// Look at discovery interrupts if not yet connected.
	//
	TX_DEBUG_PRINT(("Drv: SiiMhlTxDeviceIsr POWER_STATE_D0_MHL %d\n", fwPowerState));
	if (POWER_STATE_D0_MHL != fwPowerState) {
		//
		// Check important RGND, MHL_EST, CBUS_LOCKOUT and SCDT interrupts
		// During D3 we only get RGND but same ISR can work for both states
		//
		if (I2C_INACCESSIBLE == Int4Isr())
			return;
	} else if (POWER_STATE_D0_MHL == fwPowerState) {
Loop:
#ifdef TSRC_RSEN_DEGLITCH_INCLUDED_IN_TSRC_RSEN_CHK //(
		if (I2C_INACCESSIBLE == Int1Isr())
			return;
#else //(
		//
		// Check RSEN LOW interrupt and apply deglitch timer for transition
		// from connected to disconnected state.
		//
		if (timer_RSEN_CHK_Expired) {
			uint8_t rsen  = I2C_ReadByte(PAGE_0_0X72, 0x09) & BIT2;
			//
			// If no MHL cable is connected, we may not receive interrupt for RSEN at all
			// as nothing would change. Poll the status of RSEN here.
			//
			// Also interrupt may come only once who would have started deglitch timer.
			// The following function will look for expiration of that before disconnection.
			//

			if (deglitchingRsenNow) {
				TX_DEBUG_PRINT(("Drv: deglitchingRsenNow.\n"));
				DeglitchRsenLow(rsen);
			} else
				Int1RsenIsr(rsen);
		}

#endif
		if (deglitchingRsenNow) {
			TX_DEBUG_PRINT(("Drv: deglitchingRsenNow.go to loop, deglitchingRsenNow=%d\n", deglitchingRsenNow));
			goto Loop;
		}
#ifdef	APPLY_PLL_RECOVERY
		//
		// Trigger a PLL recovery if SCDT is high or FIFO overflow has happened.
		//
		if ((MHL_STATUS_PATH_ENABLED & linkMode) && (BIT6 & dsHpdStatus) && (contentOn))
			SiiMhlTxDrvRecovery();

#endif
		if (contentOn) {
			I2C_WriteByte(PAGE_0_0X72, (0x74), BIT0);
			//
			// Check for any peer messages for DCAP_CHG etc
			// Dispatch to have the CBUS module working only once connected.
			//
			MhlCbusIsr();
			// Call back into the MHL component to give it a chance to
			// take care of any message processing caused by this interrupt.
			MhlTxProcessEvents();
			if ((timer_RSEN_CHK_Expired != true) && (fwPowerState == POWER_STATE_D0_MHL)) {
				TX_DEBUG_PRINT(("Drv: timer_RSEN_CHK_ not Expired.go to loop\n"));
				goto Loop;
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow
//
// Acquire the direct control of Upstream HPD.
//
static void SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow(void)
{

	ReadModifyWritePage0(0x79, BIT5 | BIT4, BIT4);
	TX_DEBUG_PRINT(("Drv: Upstream HPD Acquired - driven low.\n"));
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvReleaseUpstreamHPDControl
//
// Release the direct control of Upstream HPD.
//
static void SiiMhlTxDrvReleaseUpstreamHPDControl(void)
{
	// Un-force HPD (it was kept low, now propagate to source
	// let HPD float by clearing reg_hpd_out_ovr_en
	CLR_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("Drv: Upstream HPD released.\n"));
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvTmdsControl
//
// Control the TMDS output. MhlTx uses this to support RAP content on and off.
//
void	SiiMhlTxDrvTmdsControl(bool_t enable)
{
	if (enable) {
		SET_BIT(PAGE_0_0X72, 0x80, 4);
		TX_DEBUG_PRINT(("Drv: TMDS Output Enabled\n"));
		SiiMhlTxDrvReleaseUpstreamHPDControl();
	} else {
		CLR_BIT(PAGE_0_0X72, 0x80, 4);
		TX_DEBUG_PRINT(("Drv: TMDS Output Disabled\n"));
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvNotifyEdidChange
//
// MhlTx may need to inform upstream device of an EDID change. This can be
// achieved by toggling the HDMI HPD signal or by simply calling EDID read
// function.
//
void SiiMhlTxDrvNotifyEdidChange(void)
{
	TX_DEBUG_PRINT(("Drv: SiiMhlTxDrvNotifyEdidChange\n"));
	//
	// Prepare to toggle HPD to upstream
	//
	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

	HalTimerWait(110);

	SET_BIT(PAGE_0_0X72, 0x79,  5);

	SiiMhlTxDrvReleaseUpstreamHPDControl();

}
//------------------------------------------------------------------------------
// Function:    SiiMhlTxDrvSendCbusCommand
//
// Write the specified Sideband Channel command to the CBUS.
// Command can be a MSC_MSG command (RCP/RAP/RCPK/RCPE/RAPK), or another command
// such as READ_DEVCAP, SET_INT, WRITE_STAT, etc.
//
// Parameters:
//              pReq    - Pointer to a cbus_req_t structure containing the
//                        command to write
// Returns:     true    - successful write
//              false   - write failed
//------------------------------------------------------------------------------

bool_t SiiMhlTxDrvSendCbusCommand(cbus_req_t *pReq)
{
	bool_t  success = true;

	uint8_t i, startbit;

	//
	// If not connected, return with error
	//
	if ((POWER_STATE_D0_MHL != fwPowerState) || (mscCmdInProgress)) {
		TX_DEBUG_PRINT(("Error: Drv: fwPowerState: %02X, or CBUS(0x0A):%02X mscCmdInProgress = %d\n",
					(int) fwPowerState,
					(int) ReadByteCBUS(0x0a),
					(int) mscCmdInProgress));

		return false;
	}
	// Now we are getting busy
	mscCmdInProgress	= true;


	TX_DEBUG_PRINT(("Drv: Sending MSC command %02X, %02X, %02X, %02X\n",
				(int)pReq->command,
				(int)(pReq->offsetData),
				(int)pReq->payload_u.msgData[0],
				(int)pReq->payload_u.msgData[1]));

	/****************************************************************************************/
	/* Setup for the command - write appropriate registers and determine the correct        */
	/*                         start bit.                                                   */
	/****************************************************************************************/

	// Set the offset and outgoing data byte right away
	WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD    & 0xFF), pReq->offsetData);
	WriteByteCBUS((REG_CBUS_PRI_WR_DATA_1ST & 0xFF), pReq->payload_u.msgData[0]);

	startbit = 0x00;
	switch (pReq->command) {
	case MHL_SET_INT:
		startbit = MSC_START_BIT_WRITE_REG;
		break;

	case MHL_WRITE_STAT:
		startbit = MSC_START_BIT_WRITE_REG;
		break;

	case MHL_READ_DEVCAP:
		startbit = MSC_START_BIT_READ_REG;
		break;

	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command);
		startbit = MSC_START_BIT_MSC_CMD;
		break;

	case MHL_MSC_MSG:
		WriteByteCBUS((REG_CBUS_PRI_WR_DATA_2ND & 0xFF), pReq->payload_u.msgData[1]);
		WriteByteCBUS((REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command);
		startbit = MSC_START_BIT_VS_CMD;
		break;

	case MHL_WRITE_BURST:
		ReadModifyWriteCBUS((REG_MSC_WRITE_BURST_LEN & 0xFF), 0x0F, pReq->length - 1);


		if (NULL == pReq->payload_u.pdatabytes) {
			TX_DEBUG_PRINT(("Drv: Put pointer to WRITE_BURST data in req.pdatabytes!!!\n\n"));
		} else {
			uint8_t *pData = pReq->payload_u.pdatabytes;
			TX_DEBUG_PRINT(("Drv: Writing data into scratchpad\n\n"));
			for (i = 0; i < pReq->length; i++) {
				WriteByteCBUS((REG_CBUS_SCRATCHPAD_0 & 0xFF) + i, *pData++);
			}
		}
		startbit = MSC_START_BIT_WRITE_BURST;
		break;

	default:
		success = false;
		break;
	}

	/****************************************************************************************/
	/* Trigger the CBUS command transfer using the determined start bit.                    */
	/****************************************************************************************/

	if (success)
		WriteByteCBUS(REG_CBUS_PRI_START & 0xFF, startbit);
	else
		TX_DEBUG_PRINT(("Drv: SiiMhlTxDrvSendCbusCommand failed\n\n"));

	return(success);
}

bool_t SiiMhlTxDrvCBusBusy(void)
{
	return mscCmdInProgress ? true : false;
}

////////////////////////////////////////////////////////////////////
//
// L O C A L    F U N C T I O N S
//

//#ifndef TSRC_RSEN_DEGLITCH_INCLUDED_IN_TSRC_RSEN_CHK

////////////////////////////////////////////////////////////////////
// Int1RsenIsr
//
// This interrupt is used only to decide if the MHL is disconnected
// The disconnection is determined by looking at RSEN LOW and applying
// all MHL compliant disconnect timings and deglitch logic.
//
//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)
////////////////////////////////////////////////////////////////////

void Int1ProcessRsen(uint8_t rsen)
{
	//
	// RSEN becomes LOW in SYS_STAT register 0x72:0x09[2]
	// SYS_STAT	==> bit 7 = VLOW, 6:4 = MSEL, 3 = TSEL, 2 = RSEN, 1 = HPD, 0 = TCLK STABLE
	//
	// Start countdown timer for deglitch
	// Allow RSEN to stay low this much before reacting
	//
	if (rsen == 0x00) {
		//
		// We got this interrupt due to cable removal
		// Start deglitch timer
		//

		delay_in_ms = 10L;

		hr_timer_RSEN_DEGLITCH_ktime = ktime_set(0, MS_TO_NS(delay_in_ms));
		TX_DEBUG_PRINT(("Drv: Int1RsenIsr: Start T_SRC_RSEN_DEGLITCH (%d ms) before disconnection\n", (int)(delay_in_ms)));


		printk(KERN_INFO  "Starting timer to fire in T_SRC_RSEN_DEGLITCH:%ldms (%ld)\n", delay_in_ms, jiffies);

		hrtimer_start(&hr_timer_RSEN_DEGLITCH, hr_timer_RSEN_DEGLITCH_ktime, HRTIMER_MODE_REL);

		deglitchingRsenNow = true;
	} else if (deglitchingRsenNow) {
		TX_DEBUG_PRINT(("Drv: Ignore now, RSEN is high. This was a glitch.\n"));
		//
		// Ignore now, this was a glitch
		//
		deglitchingRsenNow = false;
	}
}
void	Int1RsenIsr(uint8_t rsen)
{
	uint8_t reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);
	// Look at RSEN interrupt.
	// If RSEN interrupt is lost, check if we should deglitch using the RSEN status only.
	if (reg71 & BIT5) {
		TX_DEBUG_PRINT (("Drv: Got INTR_1: from reg71 = %02X, rsen = %02X\n", (int) reg71, (int) rsen));
		Int1ProcessRsen(rsen);

		I2C_WriteByte(PAGE_0_0X72, 0x71, BIT5);
	} else if ((false == deglitchingRsenNow) && (rsen == 0x00)) {
		TX_DEBUG_PRINT (("Drv: Got INTR_1: reg71 = %02X, from rsen = %02X\n", (int) reg71, (int) rsen));
		Int1ProcessRsen(rsen);
	} else if (deglitchingRsenNow) {
		TX_DEBUG_PRINT(("Drv: Ignore now coz (reg71 & BIT5) has been cleared. This was a glitch.\n"));
		//
		// Ignore now, this was a glitch
		//
		deglitchingRsenNow = false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// DeglitchRsenLow
//
// This function looks at the RSEN signal if it is low.
//
// The disconnection will be performed only if we were in fully MHL connected
// state for more than 400ms AND a 150ms deglitch from last interrupt on RSEN
// has expired.
//
// If MHL connection was never established but RSEN was low, we unconditionally
// and instantly process disconnection.
//
static void DeglitchRsenLow(uint8_t rsen)
{
	TX_DEBUG_PRINT(("Drv: DeglitchRsenLow RSEN <72:09[2]> = %02X\n", (int) rsen));

	if (rsen == 0x00) {
		TX_DEBUG_PRINT(("Drv: RSEN is Low.\n"));
		//
		// If no MHL cable is connected or RSEN deglitch timer has started,
		// we may not receive interrupts for RSEN.
		// Monitor the status of RSEN here.
		//
		//
		// First check means we have not received any interrupts and just started
		// but RSEN is low. Case of "nothing" connected on MHL receptacle
		//
		if ((POWER_STATE_D0_MHL == fwPowerState) && timer_RSEN_DEGLITCH_Expired) {
			//PROBE_INVERT
			//TX_DEBUG_PRINT(("TIMER_TO_DO_RSEN_DEGLITCH expired\n"));
			// Second condition means we were fully operational, then a RSEN LOW interrupt
			// occured and a DEGLITCH_TIMER per MHL specs started and completed.
			// We can disconnect now.
			//
			//

			TX_DEBUG_PRINT(("Drv: Disconnection due to RSEN Low\n"));


			deglitchingRsenNow = false;


			// FP1226: Toggle MHL discovery to level the voltage to deterministic vale.
			DISABLE_DISCOVERY;
			ENABLE_DISCOVERY;
			//
			// We got here coz cable was never connected
			//
			dsHpdStatus &= ~BIT6;

			WriteByteCBUS(0x0D, dsHpdStatus);
			SiiMhlTxNotifyDsHpdChange(0);
			MhlTxDrvProcessDisconnection();

			// the call into the MHL component to take care of event processing happens in
			//  the MhlTx component in MhlTxNotifyConnection();
		}
	} else {
		//
		// Deglitch here:
		// RSEN is not low anymore. Reset the flag.
		// This flag will be now set on next interrupt.
		//
		// Stay connected
		//
		deglitchingRsenNow = false;
	}
}
//#endif
///////////////////////////////////////////////////////////////////////////
// WriteInitialRegisterValues
//
//
///////////////////////////////////////////////////////////////////////////

static void WriteInitialRegisterValues(void)
{
	TX_DEBUG_PRINT(("Drv: WriteInitialRegisterValues\n"));

	I2C_WriteByte(PAGE_1_0X7A, 0x3D, 0x3F);
	I2C_WriteByte(PAGE_2_0X92, 0x11, 0x01);
	I2C_WriteByte(PAGE_2_0X92, 0x12, 0x15);
	I2C_WriteByte(PAGE_0_0X72, 0x08, 0x35);


	CbusReset();



	I2C_WriteByte(PAGE_2_0X92, 0x10, 0xC1);
	I2C_WriteByte(PAGE_2_0X92, 0x17, 0x03);
	I2C_WriteByte(PAGE_2_0X92, 0x1A, 0x20);
	I2C_WriteByte(PAGE_2_0X92, 0x22, 0x8A);
	I2C_WriteByte(PAGE_2_0X92, 0x23, 0x6A);
	I2C_WriteByte(PAGE_2_0X92, 0x24, 0xAA);
	I2C_WriteByte(PAGE_2_0X92, 0x25, 0xCA);
	I2C_WriteByte(PAGE_2_0X92, 0x26, 0xEA);
	I2C_WriteByte(PAGE_2_0X92, 0x4C, 0xA0);
	I2C_WriteByte(PAGE_2_0X92, 0x4D, 0x00);

	I2C_WriteByte(PAGE_0_0X72, 0x80, 0x24);
	I2C_WriteByte(PAGE_2_0X92, 0x45, 0x44);
	I2C_WriteByte(PAGE_2_0X92, 0x31, 0x0A);
	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);
	I2C_WriteByte(PAGE_0_0X72, 0xA1, 0xFC);


	I2C_WriteByte(PAGE_0_0X72, 0xA3, 0xEB);
	I2C_WriteByte(PAGE_0_0X72, 0xA6, 0x0C);


	I2C_WriteByte(PAGE_0_0X72, 0x2B, 0x01);

	//
	// CBUS & Discovery
	// CBUS discovery cycle time for each drive and float = 100us
	//
	ReadModifyWritePage0(0x90, BIT3 | BIT2, BIT2);

	// Changed from 66 to 77 for 94[1:0] = 11 = 5k reg_cbusmhl_pup_sel
	// and bits 5:4 = 11 rgnd_vth_ctl
	//
	I2C_WriteByte(PAGE_0_0X72, 0x94, 0x77);


	I2C_WriteByte(PAGE_CBUS_0XC8, 0x31, I2C_ReadByte(PAGE_CBUS_0XC8, 0x31) | 0x0c);



	I2C_WriteByte(PAGE_0_0X72, 0xA5, 0xA0);

	TX_DEBUG_PRINT(("Drv: MHL 1.0 Compliant Clock\n"));



#if (CI2CA == LOW)

	I2C_WriteByte(PAGE_0_0X72, 0x91, 0xA5);
	I2C_WriteByte(PAGE_0_0X72, 0x95, 0x71);
#else

	I2C_WriteByte(PAGE_0_0X72, 0x91, 0xAD);
	I2C_WriteByte(PAGE_0_0X72, 0x95, 0x75);
	ReadModifyWritePage0(0x91, BIT3, BIT3);
	ReadModifyWritePage0(0x96, BIT5, 0x00);
#endif



	// Default is good.
	//
	// Use 1k and 2k commented.
	I2C_WriteByte(PAGE_0_0X72, 0x96, 0x22);

	// Use VBUS path of discovery state machine
	I2C_WriteByte(PAGE_0_0X72, 0x97, 0x00);


	//
	// For MHL compliance we need the following settings for register 93 and 94
	// Bug 20686
	//
	// To allow RGND engine to operate correctly.
	//
	// When moving the chip from D2 to D0 (power up, init regs) the values should be
	// 94[1:0] = 11  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	// 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be set for 10k
	// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	//

	WriteBytePage0(0x92, 0xA6);				//
	// change from CC to 8C to match 10K
	// 0b11 is 5K, 0b10 is 10K, 0b01 is 20k and 0b00 is off
	WriteBytePage0(0x93, 0x8C);

	//Jiangshanbin HPD BIT6 for push pull
	ReadModifyWritePage0(0x79, BIT6, 0x00);


	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();


	HalTimerWait(25);
	ReadModifyWritePage0(0x95, BIT6, 0x00);

	I2C_WriteByte(PAGE_0_0X72, 0x90, 0x27);

	InitCBusRegs();

	// Enable Auto soft reset on SCDT = 0
	I2C_WriteByte(PAGE_0_0X72, 0x05, 0x04);

	// HDMI Transcode mode enable
	I2C_WriteByte(PAGE_0_0X72, 0x0D, 0x1C);

	UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;

	//	pinLed3xForProbe = 0;
	//	pinProbe = 0;
}

///////////////////////////////////////////////////////////////////////////
// InitCBusRegs
//
///////////////////////////////////////////////////////////////////////////
static void InitCBusRegs(void)
{
	uint8_t regval;

	TX_DEBUG_PRINT(("Drv: InitCBusRegs\n"));
	// Increase DDC translation layer timer
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x07, 0xF2);          // new default is for MHL mode
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x40, 0x03); 			// CBUS Drive Strength
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x42, 0x06); 			// CBUS DDC interface ignore segment pointer
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x36, 0x0C);

	I2C_WriteByte(PAGE_CBUS_0XC8, 0x3D, 0xFD);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x1C, 0x01);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x1D, 0x0F);          // MSC_RETRY_FAIL_LIM

	I2C_WriteByte(PAGE_CBUS_0XC8, 0x44, 0x02);

	// Setup our devcap
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x80, DEVCAP_VAL_DEV_STATE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x81, DEVCAP_VAL_MHL_VERSION);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x82, DEVCAP_VAL_DEV_CAT);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x83, DEVCAP_VAL_ADOPTER_ID_H);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x84, DEVCAP_VAL_ADOPTER_ID_L);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x85, DEVCAP_VAL_VID_LINK_MODE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x86, DEVCAP_VAL_AUD_LINK_MODE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x87, DEVCAP_VAL_VIDEO_TYPE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x88, DEVCAP_VAL_LOG_DEV_MAP);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x89, DEVCAP_VAL_BANDWIDTH);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8A, DEVCAP_VAL_FEATURE_FLAG);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8B, DEVCAP_VAL_DEVICE_ID_H);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8C, DEVCAP_VAL_DEVICE_ID_L);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8D, DEVCAP_VAL_SCRATCHPAD_SIZE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8E, DEVCAP_VAL_INT_STAT_SIZE);
	I2C_WriteByte(PAGE_CBUS_0XC8, 0x8F, DEVCAP_VAL_RESERVED);

	// Make bits 2,3 (initiator timeout) to 1,1 for register CBUS_LINK_CONTROL_2
	regval = I2C_ReadByte(PAGE_CBUS_0XC8, REG_CBUS_LINK_CONTROL_2);
	regval = (regval | 0x0C);
	I2C_WriteByte(PAGE_CBUS_0XC8, REG_CBUS_LINK_CONTROL_2, regval);

	// Clear legacy bit on Wolverine TX.
	regval = I2C_ReadByte(PAGE_CBUS_0XC8, REG_MSC_TIMEOUT_LIMIT);
	regval &= ~MSC_TIMEOUT_LIMIT_MSB_MASK;
	regval |= 0x0F;
	I2C_WriteByte(PAGE_CBUS_0XC8, REG_MSC_TIMEOUT_LIMIT, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));

	// Set NMax to 1
	I2C_WriteByte(PAGE_CBUS_0XC8, REG_CBUS_LINK_CONTROL_1, 0x01);
	ReadModifyWriteCBUS(REG_CBUS_LINK_CONTROL_11
			, BIT5 | BIT4 | BIT3
			, BIT5 | BIT4
			);

	ReadModifyWriteCBUS(REG_MSC_TIMEOUT_LIMIT, 0x0F, 0x0D);

	ReadModifyWriteCBUS(0x2E
			, BIT4 | BIT2 | BIT0
			, BIT4 | BIT2 | BIT0
			);

}

///////////////////////////////////////////////////////////////////////////
//
// ForceUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ForceUsbIdSwitchOpen(void)
{
	DISABLE_DISCOVERY
		ReadModifyWritePage0(0x95, BIT6, BIT6);

	WriteBytePage0(0x92, 0xA6);
	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();
}

///////////////////////////////////////////////////////////////////////////
//
// ReleaseUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ReleaseUsbIdSwitchOpen(void)
{
	HalTimerWait(50);
	ReadModifyWritePage0(0x95, BIT6, 0x00);
	ENABLE_DISCOVERY;
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   CbusWakeUpPulseGenerator ()
//
// PURPOSE      :   Generate Cbus Wake up pulse sequence using GPIO or I2C method.
//
// INPUT PARAMS :   None
//
// OUTPUT PARAMS:   None
//
// GLOBALS USED :   None
//
// RETURNS      :   None
//
//////////////////////////////////////////////////////////////////////////////

static void CbusWakeUpPulseGenerator(void)
{
	TX_DEBUG_PRINT(("Drv: CbusWakeUpPulseGenerator\n"));

	//
	// I2C method
	//
	// Start the pulse

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_2 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) | 0xC0));
	HalTimerWait(T_SRC_WAKE_PULSE_WIDTH_1 - 1);

	I2C_WriteByte(PAGE_0_0X72, 0x96, (I2C_ReadByte(PAGE_0_0X72, 0x96) & 0x3F));

	HalTimerWait(T_SRC_WAKE_TO_DISCOVER);
	TX_DEBUG_PRINT(("Drv: CbusWakeUpPulseGenerator end.........\n"));

	//
	// Toggle MHL discovery bit
	//
	//	DISABLE_DISCOVERY;
	//	ENABLE_DISCOVERY;

}

///////////////////////////////////////////////////////////////////////////
//
// ApplyDdcAbortSafety
//
///////////////////////////////////////////////////////////////////////////
static void ApplyDdcAbortSafety(void)
{
	uint8_t bTemp, bPost;
#if 0
	TX_DEBUG_PRINT(("Drv: Do we need DDC Abort Safety\n"));
#endif

	WriteByteCBUS(0x29, 0xFF);  // clear the ddc abort counter
	bTemp = ReadByteCBUS(0x29);  // get the counter
	HalTimerWait(3);
	bPost = ReadByteCBUS(0x29);  // get another value of the counter

	TX_DEBUG_PRINT(("Drv: bTemp: 0x%X bPost: 0x%X\n", (int)bTemp, (int)bPost));

	if (bPost > (bTemp + 50)) {
		TX_DEBUG_PRINT(("Drv: Applying DDC Abort Safety(SWWA 18958)\n"));

		CbusReset();

		InitCBusRegs();

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();

		MhlTxDrvProcessDisconnection();
	}

}

/*
   SiiMhlTxDrvProcessRgndMhl
   optionally called by the MHL Tx Component after giving the OEM layer the
   first crack at handling the event.
 */
void SiiMhlTxDrvProcessRgndMhl(void)
{
	// Select CBUS drive float.
	SET_BIT(PAGE_0_0X72, 0x95, 5);

	TX_DEBUG_PRINT(("Drv: Waiting T_SRC_VBUS_CBUS_TO_STABLE (%d ms)\n", (int)T_SRC_VBUS_CBUS_TO_STABLE));
	HalTimerWait(T_SRC_VBUS_CBUS_TO_STABLE);

#if (VBUS_POWER_CHK == ENABLE)
	AppVbusControl(vbusPowerState = false);
#endif

	// Discovery enabled
	//	STROBE_POWER_ON

	//
	// Send slow wake up pulse using GPIO or I2C
	//
	CbusWakeUpPulseGenerator();
}
///////////////////////////////////////////////////////////////////////////
// ProcessRgnd
//
// H/W has detected impedance change and interrupted.
// We look for appropriate impedance range to call it MHL and enable the
// hardware MHL discovery logic. If not, disable MHL discovery to allow
// USB to work appropriately.
//
// In current chip a firmware driven slow wake up pulses are sent to the
// sink to wake that and setup ourselves for full D0 operation.
///////////////////////////////////////////////////////////////////////////

static void	ProcessRgnd(void)
{
	uint8_t reg99RGNDRange;
	//
	// Impedance detection has completed - process interrupt
	//
	reg99RGNDRange = I2C_ReadByte(PAGE_0_0X72, 0x99) & 0x03;
	TX_DEBUG_PRINT(("Drv: RGND Reg 99 = %02X\n", (int)reg99RGNDRange));

	//
	// Reg 0x99
	// 00, 01 or 11 means USB.
	// 10 means 1K impedance (MHL)
	//
	// If 1K, then only proceed with wake up pulses
	if (0x02 == reg99RGNDRange || 0x01 == reg99RGNDRange) {

		SwitchToD0();
		TX_DEBUG_PRINT(("(MHL Device)\n"));
		//The sequence of events during MHL discovery is as follows:
		//	(i) SiI9244 blocks on RGND interrupt (Page0:0x74[6]).
		//	(ii) System firmware turns off its own VBUS if present.
		//	(iii) System firmware waits for about 200ms (spec: TVBUS_CBUS_STABLE, 100 - 1000ms), then checks for the presence of
		//		VBUS from the Sink.
		//	(iv) If VBUS is present then system firmware proceed to drive wake pulses to the Sink as described in previous
		//		section.
		//	(v) If VBUS is absent the system firmware turns on its own VBUS, wait for an additional 200ms (spec:
		//		TVBUS_OUT_TO_STABLE, 100 - 1000ms), and then proceed to drive wake pulses to the Sink as described in above.

		// AP need to check VBUS power present or absent here 	// by oscar 20110527

		SiiMhlTxNotifyRgndMhl(); // this will call the application and then optionally call

	} else {
		TX_DEBUG_PRINT(("Drv: USB impedance. Set for USB Established.\n"));

		CLR_BIT(PAGE_0_0X72, 0x95, 5);
	}
}

////////////////////////////////////////////////////////////////////
// SwitchToD0
// This function performs s/w as well as h/w state transitions.
//
// Chip comes up in D2. Firmware must first bring it to full operation
// mode in D0.
////////////////////////////////////////////////////////////////////
static void	SwitchToD0(void)
{
	uint8_t idh = 0;

	idh = I2C_ReadByte(PAGE_0_0X72, 0x03);
	if (idh != 0x92) {
		TX_DEBUG_PRINT (("Drv: Switch To D0...Chip offline. Abort\n"));
		return;
	}

	TX_DEBUG_PRINT(("Drv: Switch To Full power mode (D0)\n"));

	timer_RSEN_CHK_Expired = false;
	timer_RSEN_DEGLITCH_Expired = false;

	//
	// WriteInitialRegisterValues switches the chip to full power mode.
	//
	TX_DEBUG_PRINT(("Drv: D2->D0  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n", (int)ReadBytePage0(0x90), (int)ReadBytePage0(0x93), (int)ReadBytePage0(0x94)));
	WriteInitialRegisterValues();
	TX_DEBUG_PRINT(("Drv: D0:  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n", (int)ReadBytePage0(0x90), (int)ReadBytePage0(0x93), (int)ReadBytePage0(0x94)));

	STROBE_POWER_ON
		fwPowerState = POWER_STATE_D0_NO_MHL;
}

////////////////////////////////////////////////////////////////////
// SwitchToD3
//
// This function performs s/w as well as h/w state transitions.
//
////////////////////////////////////////////////////////////////////
extern void CBusQueueReset(void);
static void	SwitchToD3(void)
{
	uint8_t idh = 0;

	idh = I2C_ReadByte(PAGE_0_0X72, 0x03);
	if (idh != 0x92) {
		TX_DEBUG_PRINT (("Drv: Switch To D3...Chip offline. Abort\n"));
		return;
	}

	if (POWER_STATE_D3 != fwPowerState || true) {
		TX_API_PRINT(("Drv: Switch To D3...\n"));

		timer_RSEN_CHK_Expired = false;
		timer_RSEN_DEGLITCH_Expired = false;

		ForceUsbIdSwitchOpen();

		msleep(50);

		//
		// To allow RGND engine to operate correctly.
		// So when moving the chip from D0 MHL connected to D3 the values should be
		// 94[1:0] = 00  reg_cbusmhl_pup_sel[1:0] should be set for open
		// 93[7:6] = 00  reg_cbusdisc_pup_sel[1:0] should be set for open
		// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
		//
		// Disable CBUS pull-up during RGND measurement
		ReadModifyWritePage0(0x93, BIT7 | BIT6 | BIT5 | BIT4, 0);
		//I2C_WriteByte(PAGE_0_0X72, 0x93, 0x8c);
		TX_DEBUG_PRINT(("Drv: D0:  0x90:0x%02x 0x93:0x%02x	0x94:0x%02x\n", (int)ReadBytePage0(0x90), (int)ReadBytePage0(0x93), (int)ReadBytePage0(0x94)));

		ReadModifyWritePage0(0x94, BIT1 | BIT0, 0);
		// 1.8V CBUS VTH & GND threshold

		ReleaseUsbIdSwitchOpen();

		// Force HPD to 0 when not in MHL mode.
		SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

		// Change TMDS termination to high impedance on disconnection
		// Bits 1:0 set to 11
		I2C_WriteByte(PAGE_2_0X92, 0x01, 0x03);

		// clear all interrupt here before go into D3 mode by oscar
		I2C_WriteByte(PAGE_0_0X72, 0x71, 0xFF);
		I2C_WriteByte(PAGE_0_0X72, 0x72, 0xFF);
		I2C_WriteByte(PAGE_0_0X72, 0x74, 0xBF);
		WriteByteCBUS(0x08, 0xFF);
		WriteByteCBUS(0x1E, 0xFF);

		//
		// GPIO controlled from SiIMon can be utilized to disallow
		// low power mode, thereby allowing SiIMon to debug register contents.
		// Otherwise SiIMon reads all registers as 0xFF
		//
		{
			TX_DEBUG_PRINT(("Drv: ->D3  0x90:0x%02x 0x93:0x%02x  0x94:0x%02x\n", (int)ReadBytePage0(0x90), (int)ReadBytePage0(0x93), (int)ReadBytePage0(0x94)));
			//
			// Change state to D3 by clearing bit 0 of 3D (SW_TPI, Page 1) register
			//
			CLR_BIT(PAGE_1_0X7A, 0x3D, 0);
			CBusQueueReset();

			fwPowerState = POWER_STATE_D3;
		}

		idh = I2C_ReadByte(PAGE_0_0X72, 0x03);
		if (idh != 0x92) {
			TX_DEBUG_PRINT (("Switch to D3 successfully\n"));
		} else {
			TX_DEBUG_PRINT (("Fail to Switch to D3\n"));
		}

	}
#if (VBUS_POWER_CHK == ENABLE)
	if (vbusPowerState == false)
		AppVbusControl(vbusPowerState = true);
#endif
}

void SiiMhlSwitchStatus(int status)
{
	if (status == 3) {
		SwitchToD0();
		SwitchToD3();
	}
}

////////////////////////////////////////////////////////////////////
// Int4Isr
//
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(interested during D0)
////////////////////////////////////////////////////////////////////
static int Int4Isr(void)
{
	uint8_t reg74, idh = 0;

	idh = I2C_ReadByte(PAGE_0_0X72, 0x03);
	if (idh != 0x92) {
		TX_DEBUG_PRINT (("MhlTx: Chip offline. Abort\n"));
		return false;
	}

	reg74 = I2C_ReadByte(PAGE_0_0X72, (0x74));


#if 0
	if (0xFF == reg74)
#else
		if (0xFF == reg74 || 0x87 == reg74)
#endif
			return I2C_INACCESSIBLE;


	if (reg74 & BIT2)
		MhlTxDrvProcessConnection();

	else if (reg74 & BIT3)
		MhlTxDrvProcessDisconnection();

	if ((POWER_STATE_D3 == fwPowerState) && (reg74 & BIT6)) {
		// process RGND interrupt

		// Switch to full power mode.
		//		SwitchToD0();

		//
		// If a sink is connected but not powered on, this interrupt can keep coming
		// Determine when to go back to sleep. Say after 1 second of this state.
		//
		// Check RGND register and send wake up pulse to the peer
		//
		ProcessRgnd();
	}


	if (reg74 & BIT4) {
		TX_DEBUG_PRINT(("Drv: CBus Lockout\n"));

		SwitchToD0();
		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();
		SwitchToD3();
	}
	I2C_WriteByte(PAGE_0_0X72, (0x74), reg74&0xFE);
	return I2C_ACCESSIBLE;
}

#ifdef	APPLY_PLL_RECOVERY
///////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyPllRecovery
//
// PURPOSE:		This function helps recover PLL.
//
///////////////////////////////////////////////////////////////////////////
static void ApplyPllRecovery(void)
{

	CLR_BIT(PAGE_0_0X72, 0x80, 4);


	SET_BIT(PAGE_0_0X72, 0x80, 4);


	HalTimerWait(10);


	SET_BIT(PAGE_0_0X72, 0x05, 4);

	CLR_BIT(PAGE_0_0X72, 0x05, 4);

	TX_DEBUG_PRINT(("Drv: Applied PLL Recovery\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   SiiMhlTxDrvRecovery ()
//
// PURPOSE      :   Check SCDT interrupt and PSTABLE interrupt
//
//
// DESCRIPTION :  If SCDT interrupt happened and current status
// is HIGH, irrespective of the last status (assuming we can miss an interrupt)
// go ahead and apply PLL recovery.
//
// When a PSTABLE interrupt happens, it is an indication of a possible
// FIFO overflow condition. Apply a recovery method.
//
//////////////////////////////////////////////////////////////////////////////
static void SiiMhlTxDrvRecovery(void)
{
	//
	// Detect Rising Edge of SCDT
	//
	// Check if SCDT interrupt came
	if ((I2C_ReadByte(PAGE_0_0X72, (0x74)) & BIT0)) {
		//
		// Clear this interrupt and then check SCDT.
		// if the interrupt came irrespective of what SCDT was earlier
		// and if SCDT is still high, apply workaround.
		//
		// This approach implicitly takes care of one lost interrupt.
		//
		SET_BIT(PAGE_0_0X72, (0x74), 0);


		// Read status, if it went HIGH
		if ((((I2C_ReadByte(PAGE_0_0X72, 0x81)) & BIT1) >> 1)) {
			// Help probe SCDT
			//pinLed3xForProbe = 1;

			// Toggle TMDS and reset MHL FIFO.
			ApplyPllRecovery();
		}

	}
	//
	// Check PSTABLE interrupt...reset FIFO if so.
	//
	if ((I2C_ReadByte(PAGE_0_0X72, (0x72)) & BIT1)) {

		TX_DEBUG_PRINT(("Drv: PSTABLE Interrupt\n"));


		ApplyPllRecovery();


		SET_BIT(PAGE_0_0X72, (0x72), 1);

	}
}
#endif

///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessConnection
//
///////////////////////////////////////////////////////////////////////////
static void MhlTxDrvProcessConnection(void)
{
	//bool_t	mhlConnected = true;

	TX_DEBUG_PRINT(("Drv: MHL Cable Connected. CBUS:0x0A = %02X\n", (int) ReadByteCBUS(0x0a)));

	if (POWER_STATE_D0_MHL == fwPowerState)
		return;
	// 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be set for 10k
	// change from  8C to CC to match 5K
	// 0b11 is 5K, 0b10 is 10K, 0b01 is 20k and 0b00 is off
	I2C_WriteByte(PAGE_0_0X72, 0x93, 0xCC);
	//
	// Discovery over-ride: reg_disc_ovride
	//
	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0x10);

	fwPowerState = POWER_STATE_D0_MHL;

	//
	// Legacy product operates in DDC Burst mode
	//

	// Increase DDC translation layer timer (uint8_t mode)
	// Setting DDC Byte Mode
	//
	WriteByteCBUS(0x07, 0xF2);
	// Doing it this way causes problems with playstation: ReadModifyWriteByteCBUS(0x07, BIT2,0);

	// Enable segment pointer safety
	SET_BIT(PAGE_CBUS_0XC8, 0x44, 1);

	// Change TMDS termination to 50 ohm termination (default)
	// Bits 1:0 set to 00
	I2C_WriteByte(PAGE_2_0X92, 0x01, 0x00);
	// upstream HPD status should not be allowed to rise until HPD from downstream is detected.

	// TMDS should not be enabled until RSEN is high, and HPD and PATH_EN are received

	// Keep the discovery enabled. Need RGND interrupt
	ENABLE_DISCOVERY;

	// Ignore RSEN interrupt for T_SRC_RXSENSE_CHK duration.
	// Get the timer started
	//
	//PROBE_INVERT

	delay_in_ms = 250L;

	hr_timer_RSEN_CHK_ktime = ktime_set(0, MS_TO_NS(delay_in_ms));
	// Wait T_SRC_RXSENSE_CHK ms to allow connection/disconnection to be stable (MHL 1.1 specs)
	TX_DEBUG_PRINT (("Drv: Wait T_SRC_RXSENSE_CHK (%d ms) before checking RSEN\n",
				(int) delay_in_ms));

	printk(KERN_INFO  "Starting timer to fire in T_SRC_RXSENSE_CHK  %ldms (%ld)\n", delay_in_ms, jiffies);

	hrtimer_start(&hr_timer_RSEN_CHK, hr_timer_RSEN_CHK_ktime, HRTIMER_MODE_REL);


	// Notify upper layer of cable connection
	contentOn = 1;
	SiiMhlTxNotifyConnection(true);
}

///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessDisconnection
//
///////////////////////////////////////////////////////////////////////////
static void MhlTxDrvProcessDisconnection(void)
{
	bool_t	mhlConnected = false;

	TX_DEBUG_PRINT (("Drv: MhlTxDrvProcessDisconnection\n"));


	// clear all interrupts
	//	I2C_WriteByte(PAGE_0_0X72, (0x74), I2C_ReadByte(PAGE_0_0X72, (0x74)));

	I2C_WriteByte(PAGE_0_0X72, 0xA0, 0xD0);

	//
	// Reset CBus to clear register contents
	// This may need some key reinitializations
	//
	//	CbusReset();

	// Disable TMDS
	SiiMhlTxDrvTmdsControl(false);

	if (POWER_STATE_D0_MHL == fwPowerState) {

		contentOn = 0;
		SiiMhlTxNotifyConnection(mhlConnected = false);
	}


	SwitchToD3();
}

///////////////////////////////////////////////////////////////////////////
//
// CbusReset
//
///////////////////////////////////////////////////////////////////////////

static void CbusReset(void)
{

	uint8_t idx;
	SET_BIT(PAGE_0_0X72, 0x05, 3);
	HalTimerWait(2);
	CLR_BIT(PAGE_0_0X72, 0x05, 3);

	mscCmdInProgress = false;

	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;

	for (idx = 0; idx < 4; idx++) {

		I2C_WriteByte(PAGE_CBUS_0XC8, 0xE0 + idx, 0xFF);
		I2C_WriteByte(PAGE_CBUS_0XC8, 0xF0 + idx, 0xFF);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// CBusProcessErrors
//
//
///////////////////////////////////////////////////////////////////////////
static uint8_t CBusProcessErrors(uint8_t intStatus)
{
	uint8_t result          = 0;
	uint8_t mscAbortReason  = 0;
	uint8_t ddcAbortReason  = 0;

	/* At this point, we only need to look at the abort interrupts. */

	intStatus &=  (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT);

	if (intStatus) {
		//      result = ERROR_CBUS_ABORT;		// No Retry will help

		/* If transfer abort or MSC abort, clear the abort reason register. */
		if (intStatus & BIT_DDC_ABORT) {
			result = ddcAbortReason = ReadByteCBUS(REG_DDC_ABORT_REASON);
			TX_DEBUG_PRINT(("CBUS DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason)));
		}

		if (intStatus & BIT_MSC_XFR_ABORT) {
			result = mscAbortReason = ReadByteCBUS(REG_PRI_XFR_ABORT_REASON);

			TX_DEBUG_PRINT(("CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n"));
			WriteByteCBUS(REG_PRI_XFR_ABORT_REASON, 0xFF);
		}
		if (intStatus & BIT_MSC_ABORT) {
			TX_DEBUG_PRINT(("CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n"));
			WriteByteCBUS(REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF);
		}

		// Now display the abort reason.

		if (mscAbortReason != 0) {
			TX_DEBUG_PRINT(("CBUS:: Reason for ABORT is ....0x%02X = ", (int)mscAbortReason));

			if (mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)
				TX_DEBUG_PRINT(("Requestor MAXFAIL - retry threshold exceeded\n"));
			if (mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)
				TX_DEBUG_PRINT(("Protocol Error\n"));
			if (mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)
				TX_DEBUG_PRINT(("Requestor translation layer timeout\n"));
			if (mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)
				TX_DEBUG_PRINT(("Peer sent an abort\n"));
			if (mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)
				TX_DEBUG_PRINT(("Undefined opcode\n"));
		}

	}
	return(result);
}

void SiiMhlTxDrvGetScratchPad(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	int i;
	uint8_t regOffset;

	for (regOffset = 0xC0 + startReg, i = 0; i < length; ++i, ++regOffset) {
		*pData++ = ReadByteCBUS(regOffset);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// MhlCbusIsr
//
// Only when MHL connection has been established. This is where we have the
// first looks on the CBUS incoming commands or returned data bytes for the
// previous outgoing command.
//
// It simply stores the event and allows application to pick up the event
// and respond at leisure.
//
// Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
///////////////////////////////////////////////////////////////////////////
static void MhlCbusIsr(void)
{
	uint8_t		cbusInt;
	uint8_t     gotData[4];
	uint8_t		i;
	uint8_t		reg71 = I2C_ReadByte(PAGE_0_0X72, 0x71);

	//
	// Main CBUS interrupts on CBUS_INTR_STATUS
	//
	cbusInt = ReadByteCBUS(0x08);

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if (cbusInt == 0xFF || cbusInt == 0x87)
		return;

	if (cbusInt) {
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x08, cbusInt);

		TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_1: %02X\n", (int) cbusInt));
	}

	if (cbusInt & BIT2)
		ApplyDdcAbortSafety();

	if ((cbusInt & BIT3)) {
		uint8_t mscMsg[2];
		TX_DEBUG_PRINT(("Drv: MSC_MSG Received\n"));
		//
		// Two bytes arrive at registers 0x18 and 0x19
		//
		mscMsg[0] = ReadByteCBUS(0x18);
		mscMsg[1] = ReadByteCBUS(0x19);
		if (MHL_MSC_MSG_RAP == mscMsg[0]) {
			if (MHL_RAP_CONTENT_ON == mscMsg[1])
				contentOn = 1;
			else if (MHL_RAP_CONTENT_OFF == mscMsg[1])
				contentOn = 0;
		}
		TX_DEBUG_PRINT(("Drv: MSC MSG: %02X %02X\n", (int)mscMsg[0], (int)mscMsg[1]));
		SiiMhlTxGotMhlMscMsg(mscMsg[0], mscMsg[1]);
	}
	if ((cbusInt & BIT5) || (cbusInt & BIT6)) {
		gotData[0] = CBusProcessErrors(cbusInt);
		mscCmdInProgress = false;
	}

	if (cbusInt & BIT4) {
		TX_DEBUG_PRINT(("Drv: MSC_REQ_DONE\n"));

		mscCmdInProgress = false;
		// only do this after cBusInt interrupts are cleared above
		SiiMhlTxMscCommandDone(ReadByteCBUS(0x16));
	}

	if (BIT7 & cbusInt) {
#define CBUS_LINK_STATUS_2 0x38
		TX_DEBUG_PRINT(("Drv: Clearing CBUS_link_hard_err_count\n"));

		WriteByteCBUS(CBUS_LINK_STATUS_2, (uint8_t)(ReadByteCBUS(CBUS_LINK_STATUS_2) & 0xF0));
	}
	//
	// Now look for interrupts on register0x1E. CBUS_MSC_INT2
	// 7:4 = Reserved
	//   3 = msc_mr_write_state = We got a WRITE_STAT
	//   2 = msc_mr_set_int. We got a SET_INT
	//   1 = reserved
	//   0 = msc_mr_write_burst. We received WRITE_BURST
	//
	cbusInt = ReadByteCBUS(0x1E);
	if (cbusInt) {
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x1E, cbusInt);

		TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_2: %02X\n", (int) cbusInt));
	}
	if (BIT0 & cbusInt) {

		SiiMhlTxMscWriteBurstDone(cbusInt);
	}
	if (cbusInt & BIT2) {
		uint8_t intr[4];
		uint8_t address;

		TX_DEBUG_PRINT(("Drv: MHL INTR Received\n"));
		for (i = 0, address = 0xA0; i < 4; ++i, ++address) {

			intr[i] = ReadByteCBUS(address);
			WriteByteCBUS(address, intr[i]);
		}

		SiiMhlTxGotMhlIntr(intr[0], intr[1]);

	}


	{
		uint8_t status[4];
		uint8_t address;

		for (i = 0, address = 0xB0; i < 4; ++i, ++address) {

			status[i] = ReadByteCBUS(address);
			WriteByteCBUS(address , 0xFF /* future status[i] */);
		}
		linkMode = status[1];
		SiiMhlTxGotMhlStatus(status[0], status[1]);

	}
	if (reg71) {
		TX_DEBUG_PRINT(("Drv: INTR_1 @72:71 = %02X enable @72:75 = %02X\n", (int) reg71, (int)I2C_ReadByte(PAGE_0_0X72, 0x75)));

		I2C_WriteByte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK);
	}
	//
	// Check if a SET_HPD came from the downstream device.
	//
	cbusInt = ReadByteCBUS(0x0D);

	// CBUS_HPD status bit
	if (BIT6 & (dsHpdStatus ^ cbusInt)) {
		uint8_t status = cbusInt & BIT6;
		TX_DEBUG_PRINT(("Drv: Downstream HPD changed to: %02X\n", (int) cbusInt));


		SiiMhlTxNotifyDsHpdChange(status);

		if (status)
			SiiMhlTxDrvReleaseUpstreamHPDControl();


		dsHpdStatus = cbusInt;
	}
}
/*
   SiMhlTxDrvSetClkMode
   -- Set the hardware this this clock mode.
 */
void SiMhlTxDrvSetClkMode(uint8_t clkMode)
{
	clkMode = clkMode;
	TX_DEBUG_PRINT(("SiMhlTxDrvSetClkMode:0x%02x\n", (int)clkMode));


}


#if (VBUS_POWER_CHK == ENABLE)
///////////////////////////////////////////////////////////////////////////////
//
// Function Name: MHLSinkOrDonglePowerStatusCheck()
//
// Function Description: Check MHL device is dongle or sink.
//
void MHLSinkOrDonglePowerStatusCheck(void)
{
	uint8_t RegValue;

	if (POWER_STATE_D0_MHL == fwPowerState) {
		WriteByteCBUS(REG_CBUS_PRI_ADDR_CMD, MHL_DEV_CATEGORY_OFFSET);
		WriteByteCBUS(REG_CBUS_PRI_START, MSC_START_BIT_READ_REG);

		RegValue = ReadByteCBUS(REG_CBUS_PRI_RD_DATA_1ST);
		TX_DEBUG_PRINT(("[MHL]: Device Category register=0x%02X...\n", (int)RegValue));

		if (MHL_DEV_CAT_DONGLE == (RegValue & 0x0F)) {
			TX_DEBUG_PRINT(("[MHL]: DevTypeValue=0x%02X, limit the VBUS current input from dongle to be 100mA...\n", (int)RegValue));
		} else if (MHL_DEV_CAT_SINK == (RegValue & 0x0F)) {
			TX_DEBUG_PRINT(("[MHL]: DevTypeValue=0x%02X, limit the VBUS current input from sink to be 500mA...\n", (int)RegValue));
		}
	}
}

#endif
