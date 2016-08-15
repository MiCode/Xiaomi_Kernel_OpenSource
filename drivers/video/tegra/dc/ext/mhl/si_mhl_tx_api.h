/**********************************************************************************/
/*  Copyright (c) 2011, Silicon Image, Inc.  All rights reserved.                 */
/*  No part of this work may be reproduced, modified, distributed, transmitted,   */
/*  transcribed, or translated into any language or computer format, in any form  */
/*  or by any means without written permission of: Silicon Image, Inc.,           */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                          */
/**********************************************************************************/
/*
   @file si_mhl_tx_api.h
 */
#include<linux/kernel.h>
#include "sii_9244_api.h"

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxInitialize
//
// Sets the transmitter component firmware up for operation, brings up chip
// into power on state first and then back to reduced-power mode D3 to conserve
// power until an MHL cable connection has been established. If the MHL port is
// used for USB operation, the chip and firmware continue to stay in D3 mode.
// Only a small circuit in the chip observes the impedance variations to see if
// processor should be interrupted to continue MHL discovery process or not.
//
//
// All device events will result in call to the function SiiMhlTxDeviceIsr()
// by host's hardware or software (a master interrupt handler in host software
// can call it directly).  This implies that the MhlTx component shall make use
// of AppDisableInterrupts() and AppRestoreInterrupts() for any critical section
// work to prevent concurrency issues.
//
// Parameters
//
// pollIntervalMs		This number should be higher than 0 and lower than
//						51 milliseconds for effective operation of the firmware.
//						A higher number will only imply a slower response to an
//						event on MHL side which can lead to violation of a
//						connection disconnection related timing or a slower
//						response to RCP messages.
//
//
//
//
bool_t SiiMhlTxInitialize(uint8_t pollIntervalMs);


#define	MHL_TX_EVENT_NONE				0x00	/* No event worth reporting.  */
#define	MHL_TX_EVENT_DISCONNECTION		0x01	/* MHL connection has been lost */
#define	MHL_TX_EVENT_CONNECTION			0x02	/* MHL connection has been established */
#define	MHL_TX_EVENT_RCP_READY			0x03	/* MHL connection is ready for RCP */
#define	MHL_TX_EVENT_RCP_RECEIVED		0x04	/* Received an RCP. Key Code in "eventParameter" */
#define	MHL_TX_EVENT_RCPK_RECEIVED		0x05	/* Received an RCPK message */
#define	MHL_TX_EVENT_RCPE_RECEIVED		0x06	/* Received an RCPE message .*/
#define	MHL_TX_EVENT_DCAP_CHG			0x07	/* Received DCAP_CHG interrupt */
#define	MHL_TX_EVENT_DSCR_CHG			0x08	/* Received DSCR_CHG interrupt */
#define	MHL_TX_EVENT_POW_BIT_CHG		0x09	/* Peer's power capability has changed */
#define	MHL_TX_EVENT_RGND_MHL			0x0A	/* RGND measurement has determine that the peer is an MHL device */

typedef enum {
	MHL_TX_EVENT_STATUS_HANDLED = 0
	, MHL_TX_EVENT_STATUS_PASSTHROUGH
} MhlTxNotifyEventsStatus_e;
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpSend
//
// This function checks if the peer device supports RCP and sends rcpKeyCode. The
// function will return a value of true if it could successfully send the RCP
// subcommand and the key code. Otherwise false.
//
bool_t SiiMhlTxRcpSend(uint8_t rcpKeyCode);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpkSend
//
// This function checks if the peer device supports RCP and sends RCPK response
// when application desires.
// The function will return a value of true if it could successfully send the RCPK
// subcommand. Otherwise false.
//
bool_t SiiMhlTxRcpkSend(uint8_t rcpKeyCode);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpeSend
//
// The function will return a value of true if it could successfully send the RCPE
// subcommand. Otherwise false.
//
// When successful, MhlTx internally sends RCPK with original (last known)
// keycode.
//
bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxSetPathEn
//
bool_t SiiMhlTxSetPathEn(void);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxClrPathEn
//
bool_t SiiMhlTxClrPathEn(void);

///////////////////////////////////////////////////////////////////////////////
//
// AppMhlTxDisableInterrupts
//
// This function or macro is invoked from MhlTx driver to secure the processor
// before entering into a critical region.
//
// Application module must provide this function.
//
extern	void	AppMhlTxDisableInterrupts(void);

///////////////////////////////////////////////////////////////////////////////
//
// AppMhlTxRestoreInterrupts
//
// This function or macro is invoked from MhlTx driver to secure the processor
// before entering into a critical region.
//
// Application module must provide this function.
//
extern	void	AppMhlTxRestoreInterrupts(void);


///////////////////////////////////////////////////////////////////////////////
//
// AppVbusControl
//
// This function or macro is invoked from MhlTx driver to ask application to
// control the VBUS power. If powerOn is sent as non-zero, one should assume
// peer does not need power so quickly remove VBUS power.
//
// if value of "powerOn" is 0, then application must turn the VBUS power on
// within 50ms of this call to meet MHL specs timing.
//
// Application module must provide this function.
//
extern	void	AppVbusControl(bool_t powerOn);

///////////////////////////////////////////////////////////////////////////////
//
// AppNotifyMhlEnabledStatusChange
//
//  This function is invoked from the MhlTx driver to notify the application about
//  changes to the Enabled state of the MHL subsystem.
//
// Application module must provide this function.
//
void  AppNotifyMhlEnabledStatusChange(bool_t enabled);

///////////////////////////////////////////////////////////////////////////////
//
// AppNotifyMhlDownStreamHPDStatusChange
//
//  This function is invoked from the MhlTx component to notify the application about
//  changes to the Downstream HPD state of the MHL subsystem.
//
// Application module must provide this function.
//
void  AppNotifyMhlDownStreamHPDStatusChange(bool_t connected);

///////////////////////////////////////////////////////////////////////////////
//
// AppNotifyMhlEvent
//
//  This function is invoked from the MhlTx component to notify the application
//  about detected events that may be of interest to it.
//
// Application module must provide this function.
//
MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam);

/*

	AppResetMhlTx
		- reset the chip in board dependent fashion
 */
//void AppResetMhlTx(uint16_t hwResetPeriod,uint16_t hwResetDelay);

typedef enum {
	SCRATCHPAD_FAIL = -4
	, SCRATCHPAD_BAD_PARAM = -3
	, SCRATCHPAD_NOT_SUPPORTED = -2
	, SCRATCHPAD_BUSY = -1
	, SCRATCHPAD_SUCCESS = 0
} ScratchPadStatus_e;

/////////////////////////////////////////////////////////////////////////
// SiiMhlTxRequestWriteBurst
//
//  This routine is used to initiate bulk CBUS transfers (Write Bursts)
//
ScratchPadStatus_e SiiMhlTxRequestWriteBurst(uint8_t startReg, uint8_t length, uint8_t *pData);

/////////////////////////////////////////////////////////////////////////
//
// MhlTxCBusBusy
//
//  returns false when it is OK to send a CBUS command.
//
bool_t MhlTxCBusBusy(void);

///////////////////////////////////////////////////////////////////////////////
//
// MhlTxProcessEvents
//
// This internal function is called by the driver layer during interrupt processing.
// It's purpose is to process events detected during the interrupt.  Some events are
// internally handled here but most are handled by a notification to the application
// layer.
//
void MhlTxProcessEvents(void);

/////////////////////////////////////////////////////////////////////////
//
// SiiTxReadConnectionStatus
//
//  returns 1 if fully connected
//          0 otherwise
//
uint8_t    SiiTxReadConnectionStatus(void);


/*
  SiiMhlTxSetPreferredPixelFormat

	clkMode - the preferred pixel format for the CLK_MODE status register

	Returns: 0 -- success
		     1 -- failure - bits were specified that are not within the mask
 */
uint8_t SiiMhlTxSetPreferredPixelFormat(uint8_t clkMode);

/*
	SiiTxGetPeerDevCapEntry

	Parameters:
		index -- the devcap index to get
		*pData pointer to location to write data
	returns
		0 -- success
		1 -- busy.
 */
uint8_t SiiTxGetPeerDevCapEntry(uint8_t index, uint8_t *pData);

/*
	SiiGetScratchPadVector

	Parameters:
		offset -- The beginning offset into the scratch pad from which to fetch entries.
		length -- The number of entries to fetch
		*pData -- A pointer to an array of bytes where the data should be placed.

	returns:
		0 -- success
		< 0  -- error (negative numbers)

*/
ScratchPadStatus_e SiiGetScratchPadVector(uint8_t offset, uint8_t length, uint8_t *pData);

void MHLSinkOrDonglePowerStatusCheck (void);


/*
	SiiMhlTxHwReset
		This routine percolates the call to reset the Mhl Tx chip up to the application layer.
 */
 #define ENABLE_TX_DEBUG_PRINT 1
//void SiiMhlTxHwReset(uint16_t hwResetPeriod,uint16_t hwResetDelay);

#define DISABLE 0x00
#define ENABLE  0xFF

// Compile debug prints inline or not
#define CONF__TX_API_PRINT   	(ENABLE)
#define CONF__TX_DEBUG_PRINT    (ENABLE)
/*\
| | Debug Print Macro
| |
| | Note: TX_DEBUG_PRINT Requires double parenthesis
| | Example:  TX_DEBUG_PRINT(("hello, world!\n"));
\*/

#if (CONF__TX_DEBUG_PRINT == ENABLE)
    #define TX_DEBUG_PRINT(x)	printk x
#else
    #define TX_DEBUG_PRINT(x)
#endif

#if (CONF__TX_API_PRINT == ENABLE)
    #define TX_API_PRINT(x)	printk x
#else
    #define TX_API_PRINT(x)
#endif

