/*
 * SiIxxxx <Firmware or Driver>
 *
 * Copyright (C) 2011 Silicon Image Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sii_9244_api.h"

#ifndef __SII_9244_DRIVER_H__
#define __SII_9244_DRIVER_H__

//====================================================
// External Analog USB switch control signal(CI2CA) polarity select
//====================================================
#define CI2CA true

///////////////////////////////////////////////////////////////////////////////
//
// I2C Slave addresses for four pages used by Sii 9244
//
#if defined(CI2CA)
#define	PAGE_0_0X72			0x72
#define	PAGE_1_0X7A			0x7A
#define	PAGE_2_0X92			0x92
#define	PAGE_CBUS_0XC8		0xC8
#else
#define	PAGE_0_0X72			0x76
#define	PAGE_1_0X7A			0x7E
#define	PAGE_2_0X92			0x96
#define	PAGE_CBUS_0XC8		0xCC
#endif

///////////////////////////////////////////////////////////////////////////////
//
// MHL Timings applicable to this driver.
//
//
#define	T_SRC_VBUS_CBUS_TO_STABLE	(200)
#define	T_SRC_WAKE_PULSE_WIDTH_1	(20)
#define	T_SRC_WAKE_PULSE_WIDTH_2	(60)

#define	T_SRC_WAKE_TO_DISCOVER		(500)

#define	T_SRC_RSEN_DEGLITCH			(100)

#define	T_SRC_RXSENSE_CHK				(400)

#define	T_SWWA_WRITE_STAT			(50)

#define	T_SRC_DISCOVER_TO_MHL_EST	(1000)

//====================================================
// VBUS power check for supply or charge
//====================================================
#define 	VBUS_POWER_CHK				(ENABLE)


#define	T_SRC_VBUS_POWER_CHK			(2000)

//------------------------------------------------------------------------------
// Driver API typedefs
//------------------------------------------------------------------------------
//
// structure to hold operating information of MhlTx component
//
typedef struct {
	bool_t interruptDriven;
	uint8_t pollIntervalMs;

	uint8_t status_0;
	uint8_t status_1;

	uint8_t connectedReady;
	uint8_t linkMode;
	uint8_t mhlHpdStatus;
	uint8_t mhlRequestWritePending;

	bool_t mhlConnectionEvent;
	uint8_t mhlConnected;

	uint8_t mhlHpdRSENflags;

	bool_t mscMsgArrived;
	uint8_t mscMsgSubCommand;
	uint8_t mscMsgData;

	uint8_t mscFeatureFlag;

	uint8_t cbusReferenceCount;
	uint8_t mscLastCommand;
	uint8_t mscLastOffset;
	uint8_t mscLastData;

	uint8_t mscMsgLastCommand;
	uint8_t mscMsgLastData;
	uint8_t mscSaveRcpKeyCode;

	uint8_t localScratchPad[16];
	uint8_t miscFlags;


} mhlTx_config_t;


typedef enum {
	MHL_HPD = 0x01, MHL_RSEN = 0x02
} MhlHpdRSEN_e;

typedef enum {
	FLAGS_SCRATCHPAD_BUSY = 0x01, FLAGS_REQ_WRT_PENDING =
	    0x02, FLAGS_WRITE_BURST_PENDING = 0x04, FLAGS_RCP_READY =
	    0x08, FLAGS_HAVE_DEV_CATEGORY = 0x10, FLAGS_HAVE_DEV_FEATURE_FLAGS =
	    0x20, FLAGS_SENT_DCAP_RDY = 0x40, FLAGS_SENT_PATH_EN = 0x80
} MiscFlags_e;

//
// structure to hold command details from upper layer to CBUS module
//
typedef struct {
	uint8_t reqStatus;
	uint8_t retryCount;
	uint8_t command;
	uint8_t offsetData;
	uint8_t length;
	union {
		uint8_t msgData[16];
		unsigned char *pdatabytes;
	} payload_u;

} cbus_req_t;

//
// Functions that driver exposes to the upper layer.
//
//////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxChipInitialize
//
// Chip specific initialization.
// This function is for SiI 9244 Initialization: HW Reset, Interrupt enable.
//
//
//////////////////////////////////////////////////////////////////////////////
bool_t SiiMhlTxChipInitialize(void);

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDeviceIsr
//
// This function must be called from a master interrupt handler or any polling
// loop in the host software if during initialization call the parameter
// interruptDriven was set to true. SiiMhlTxGetEvents will not look at these
// events assuming firmware is operating in interrupt driven mode. MhlTx component
// performs a check of all its internal status registers to see if a hardware event
// such as connection or disconnection has happened or an RCP message has been
// received from the connected device. Due to the interruptDriven being true,
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
//              RGND            = to wake up from D3
//              MHL_EST         = connection establishment
//              CBUS_LOCKOUT= Service USB switch
//              RSEN_LOW        = Disconnection deglitcher
//              CBUS            = responder to peer messages
//                                        Especially for DCAP etc time based events
//
void SiiMhlTxDeviceIsr(void);

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDrvSendCbusCommand
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
///////////////////////////////////////////////////////////////////////////////
bool_t SiiMhlTxDrvSendCbusCommand(cbus_req_t * pReq);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvCBusBusy
//
//  returns false when the CBus is ready for the next command
bool_t SiiMhlTxDrvCBusBusy(void);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDriverTmdsControl
//
// Control the TMDS output. MhlTx uses this to support RAP content on and off.
//
void SiiMhlTxDrvTmdsControl(bool_t enable);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvNotifyEdidChange
//
// MhlTx may need to inform upstream device of a EDID change. This can be
// achieved by toggling the HDMI HPD signal or by simply calling EDID read
// function.
//
//void  SiiMhlTxDrvNotifyEdidChange ( void );
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxReadDevcap
//
// This function sends a READ DEVCAP MHL command to the peer.
// It  returns true if successful in doing so.
//
// The value of devcap should be obtained by making a call to SiiMhlTxGetEvents()
//
// offset               Which byte in devcap register is required to be read. 0..0x0E
//
bool_t SiiMhlTxReadDevcap(uint8_t offset);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvGetScratchPad
//
// This function reads the local scratchpad into a local memory buffer
//
void SiiMhlTxDrvGetScratchPad(uint8_t startReg, uint8_t * pData,
			      uint8_t length);

//
// Functions that driver expects from the upper layer.
//

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxNotifyDsHpdChange
//
// Informs MhlTx component of a Downstream HPD change (when h/w receives
// SET_HPD or CLEAR_HPD).
//
extern void SiiMhlTxNotifyDsHpdChange(uint8_t dsHpdStatus);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxNotifyConnection
//
// This function is called by the driver to inform of connection status change.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
extern void SiiMhlTxNotifyConnection(bool_t mhlConnected);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxMscCommandDone
//
// This function is called by the driver to inform of completion of last command.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
extern void SiiMhlTxMscCommandDone(uint8_t data1);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxMscWriteBurstDone
//
// This function is called by the driver to inform of completion of a write burst.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
extern void SiiMhlTxMscWriteBurstDone(uint8_t data1);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlIntr
//
// This function is called by the driver to inform of arrival of a MHL INTERRUPT.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
extern void SiiMhlTxGotMhlIntr(uint8_t intr_0, uint8_t intr_1);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlStatus
//
// This function is called by the driver to inform of arrival of a MHL STATUS.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
extern void SiiMhlTxGotMhlStatus(uint8_t status_0, uint8_t status_1);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlMscMsg
//
// This function is called by the driver to inform of arrival of a MHL STATUS.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
// Application shall not call this function.
//
extern void SiiMhlTxGotMhlMscMsg(uint8_t subCommand, uint8_t cmdData);
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlWriteBurst
//
// This function is called by the driver to inform of arrival of a scratchpad data.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
// Application shall not call this function.
//
extern void SiiMhlTxGotMhlWriteBurst(uint8_t * spadArray);

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
// Parameters
// interruptDriven              Description
//                                              If true, MhlTx component will not look at its status
//                                              registers in a polled manner from timer handler
//                                              (SiiMhlTxGetEvents). It will expect that all device
//                                              events will result in call to the function
//                                              SiiMhlTxDeviceIsr() by host's hardware or software
//                                              (a master interrupt handler in host software can cal
//                                              it directly). interruptDriven == true also implies that
//                                              the MhlTx component shall make use of AppDisableInterrupts()
//                                              and AppRestoreInterrupts() for any critical section work to
//                                              prevent concurrency issues.
//
//                                              When interruptDriven == false, MhlTx component will do
//                                              all chip status analysis via looking at its register
//                                              when called periodically into the function
//                                              SiiMhlTxGetEvents() described below.
//
// pollIntervalMs               This number should be higher than 0 and lower than
//                                              51 milliseconds for effective operation of the firmware.
//                                              A higher number will only imply a slower response to an
//                                              event on MHL side which can lead to violation of a
//                                              connection disconnection related timing or a slower
//                                              response to RCP messages.
//
//
//
//
void SiiMhlTxInitialize(bool_t interruptDriven, uint8_t pollIntervalMs);

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGetEvents
//
// This is a function in MhlTx that must be called by application in a periodic
// fashion. The accuracy of frequency (adherence to the parameter pollIntervalMs)
// will determine adherence to some timings in the MHL specifications, however,
// MhlTx component keeps a tolerance of up to 50 milliseconds for most of the
// timings and deploys interrupt disabled mode of operation (applicable only to
// Sii 9244) for creating precise pulse of smaller duration such as 20 ms.
//
// This function does not return anything but it does modify the contents of the
// two pointers passed as parameter.
//
// It is advantageous for application to call this function in task context so
// that interrupt nesting or concurrency issues do not arise. In addition, by
// collecting the events in the same periodic polling mechanism prevents a call
// back from the MhlTx which can result in sending yet another MHL message.
//
// An example of this is responding back to an RCP message by another message
// such as RCPK or RCPE.
//
void SiiMhlTxGetEvents(uint8_t * event, uint8_t * eventParameter);

//
// *event               MhlTx returns a value in this field when function completes execution.
//                              If this field is 0, the next parameter is undefined.
//                              The following values may be returned.
//
//
#define		MHL_TX_EVENT_NONE			0x00	/* No event worth reporting.  */
#define		MHL_TX_EVENT_DISCONNECTION	0x01	/* MHL connection has been lost */
#define		MHL_TX_EVENT_CONNECTION		0x02	/* MHL connection has been established */
#define		MHL_TX_EVENT_RCP_READY		0x03	/* MHL connection is ready for RCP */
//
#define		MHL_TX_EVENT_RCP_RECEIVED	0x04	/* Received an RCP. Key Code in "eventParameter" */
#define		MHL_TX_EVENT_RCPK_RECEIVED	0x05	/* Received an RCPK message */
#define		MHL_TX_EVENT_RCPE_RECEIVED	0x06	/* Received an RCPE message . */

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
// MHLPowerStatusCheck
//
// The function check dongle power status then to decide output power or not.
//
void MHLPowerStatusCheck(void);

///////////////////////////////////////////////////////////////////////////////
//
// CBUS register defintions
//
#define REG_CBUS_INTR_STATUS            0x08
#define BIT_DDC_ABORT                   (BIT2)	/* Responder aborted DDC command at translation layer */
#define BIT_MSC_MSG_RCV                 (BIT3)	/* Responder sent a VS_MSG packet (response data or command.) */
#define BIT_MSC_XFR_DONE                (BIT4)	/* Responder sent ACK packet (not VS_MSG) */
#define BIT_MSC_XFR_ABORT               (BIT5)	/* Command send aborted on TX side */
#define BIT_MSC_ABORT                   (BIT6)	/* Responder aborted MSC command at translation layer */

#define REG_CBUS_INTR_ENABLE            0x09

#define REG_DDC_ABORT_REASON        	  0x0C
#define REG_CBUS_BUS_STATUS              0x0A
#define BIT_BUS_CONNECTED                   0x01
#define BIT_LA_VAL_CHG                      	   0x02

#define REG_PRI_XFR_ABORT_REASON        0x0D

#define REG_CBUS_PRI_FWR_ABORT_REASON   		0x0E
#define	CBUSABORT_BIT_REQ_MAXFAIL			(0x01 << 0)
#define	CBUSABORT_BIT_PROTOCOL_ERROR		(0x01 << 1)
#define	CBUSABORT_BIT_REQ_TIMEOUT			(0x01 << 2)
#define	CBUSABORT_BIT_UNDEFINED_OPCODE		(0x01 << 3)
#define	CBUSSTATUS_BIT_CONNECTED			(0x01 << 6)
#define	CBUSABORT_BIT_PEER_ABORTED			(0x01 << 7)

#define REG_CBUS_PRI_START              				0x12
#define BIT_TRANSFER_PVT_CMD          				0x01
#define BIT_SEND_MSC_MSG                    			0x02
#define	MSC_START_BIT_MSC_CMD			 	(0x01 << 0)
#define	MSC_START_BIT_VS_CMD		        	 	(0x01 << 1)
#define	MSC_START_BIT_READ_REG		   	 	(0x01 << 2)
#define	MSC_START_BIT_WRITE_REG		        	(0x01 << 3)
#define	MSC_START_BIT_WRITE_BURST	        	(0x01 << 4)

#define REG_CBUS_PRI_ADDR_CMD           0x13
#define REG_CBUS_PRI_WR_DATA_1ST        0x14
#define REG_CBUS_PRI_WR_DATA_2ND        0x15
#define REG_CBUS_PRI_RD_DATA_1ST        0x16
#define REG_CBUS_PRI_RD_DATA_2ND        0x17

#define REG_CBUS_PRI_VS_CMD             0x18
#define REG_CBUS_PRI_VS_DATA            0x19

#define	REG_MSC_WRITE_BURST_LEN         0x20
#define	MSC_REQUESTOR_DONE_NACK         	(0x01 << 6)

#define	REG_CBUS_MSC_RETRY_INTERVAL			0x1A
#define	REG_CBUS_DDC_FAIL_LIMIT				0x1C
#define	REG_CBUS_MSC_FAIL_LIMIT				0x1D
#define	REG_CBUS_MSC_INT2_STATUS        	0x1E
#define REG_CBUS_MSC_INT2_ENABLE             	0x1F
#define	MSC_INT2_REQ_WRITE_MSC              (0x01 << 0)
#define	MSC_INT2_HEARTBEAT_MAXFAIL          (0x01 << 1)

#define	REG_MSC_WRITE_BURST_LEN         0x20

#define	REG_MSC_HEARTBEAT_CONTROL       0x21
#define	MSC_HEARTBEAT_PERIOD_MASK		    0x0F
#define	MSC_HEARTBEAT_FAIL_LIMIT_MASK	    0x70
#define	MSC_HEARTBEAT_ENABLE			    0x80

#define REG_MSC_TIMEOUT_LIMIT           0x22
#define	MSC_TIMEOUT_LIMIT_MSB_MASK	        (0x0F)
#define	MSC_LEGACY_BIT					    (0x01 << 7)

#define	REG_CBUS_LINK_CONTROL_1				0x30
#define	REG_CBUS_LINK_CONTROL_2				0x31
#define	REG_CBUS_LINK_CONTROL_3				0x32
#define	REG_CBUS_LINK_CONTROL_4				0x33
#define	REG_CBUS_LINK_CONTROL_5				0x34
#define	REG_CBUS_LINK_CONTROL_6				0x35
#define	REG_CBUS_LINK_CONTROL_7				0x36
#define REG_CBUS_LINK_STATUS_1          			0x37
#define REG_CBUS_LINK_STATUS_2          			0x38
#define	REG_CBUS_LINK_CONTROL_8				0x39
#define	REG_CBUS_LINK_CONTROL_9				0x3A
#define	REG_CBUS_LINK_CONTROL_10				0x3B
#define	REG_CBUS_LINK_CONTROL_11				0x3C
#define	REG_CBUS_LINK_CONTROL_12				0x3D

#define REG_CBUS_LINK_CTRL9_0           			0x3A
#define REG_CBUS_LINK_CTRL9_1           			0xBA

#define	REG_CBUS_DRV_STRENGTH_0				0x40
#define	REG_CBUS_DRV_STRENGTH_1				0x41
#define	REG_CBUS_ACK_CONTROL					0x42
#define	REG_CBUS_CAL_CONTROL					0x43

#define REG_CBUS_SCRATCHPAD_0           			0xC0
#define REG_CBUS_DEVICE_CAP_0           			0x80
#define REG_CBUS_DEVICE_CAP_1           			0x81
#define REG_CBUS_DEVICE_CAP_2           			0x82
#define REG_CBUS_DEVICE_CAP_3           			0x83
#define REG_CBUS_DEVICE_CAP_4           			0x84
#define REG_CBUS_DEVICE_CAP_5           			0x85
#define REG_CBUS_DEVICE_CAP_6           			0x86
#define REG_CBUS_DEVICE_CAP_7           			0x87
#define REG_CBUS_DEVICE_CAP_8           			0x88
#define REG_CBUS_DEVICE_CAP_9           			0x89
#define REG_CBUS_DEVICE_CAP_A           			0x8A
#define REG_CBUS_DEVICE_CAP_B           			0x8B
#define REG_CBUS_DEVICE_CAP_C           			0x8C
#define REG_CBUS_DEVICE_CAP_D           			0x8D
#define REG_CBUS_DEVICE_CAP_E           			0x8E
#define REG_CBUS_DEVICE_CAP_F           			0x8F
#define REG_CBUS_SET_INT_0						0xA0
#define REG_CBUS_SET_INT_1						0xA1
#define REG_CBUS_SET_INT_2						0xA2
#define REG_CBUS_SET_INT_3						0xA3
#define REG_CBUS_WRITE_STAT_0        			0xB0
#define REG_CBUS_WRITE_STAT_1        			0xB1
#define REG_CBUS_WRITE_STAT_2        			0xB2
#define REG_CBUS_WRITE_STAT_3        			0xB3

#define MHL_DEV_UNPOWERED		0x00
#define MHL_DEV_INACTIVE		0x01
#define MHL_DEV_QUIET			0x03
#define MHL_DEV_ACTIVE			0x04


#define MHL_VER_MAJOR			(0x01 << 4)
#define MHL_VER_MINOR			0x00
#define MHL_VERSION				(MHL_VER_MAJOR | MHL_VER_MINOR)


#define	MHL_DEV_CATEGORY_OFFSET				0x02
#define	MHL_DEV_CATEGORY_POW_BIT			(BIT4)

#define	MHL_DEV_CAT_SOURCE					0x00
#define	MHL_DEV_CAT_SINGLE_INPUT_SINK		0x01
#define	MHL_DEV_CAT_MULTIPLE_INPUT_SINK		0x02
#define	MHL_DEV_CAT_UNPOWERED_DONGLE		0x03
#define	MHL_DEV_CAT_SELF_POWERED_DONGLE	0x04
#define	MHL_DEV_CAT_HDCP_REPEATER			0x05
#define	MHL_DEV_CAT_NON_DISPLAY_SINK		0x06
#define	MHL_DEV_CAT_POWER_NEUTRAL_SINK		0x07
#define	MHL_DEV_CAT_OTHER					0x80

#define	MHL_DEV_VID_LINK_SUPPRGB444			0x01
#define	MHL_DEV_VID_LINK_SUPPYCBCR444		0x02
#define	MHL_DEV_VID_LINK_SUPPYCBCR422		0x04
#define	MHL_DEV_VID_LINK_PPIXEL				0x08
#define	MHL_DEV_VID_LINK_SUPP_ISLANDS		0x10

#define	MHL_DEV_AUD_LINK_2CH					0x01
#define	MHL_DEV_AUD_LINK_8CH					0x02

#define	MHL_DEV_FEATURE_FLAG_OFFSET			0x0A
#define	MHL_FEATURE_RCP_SUPPORT				BIT0
#define	MHL_FEATURE_RAP_SUPPORT				BIT1
#define	MHL_FEATURE_SP_SUPPORT				BIT2

#define  MHL_VT_GRAPHICS						0x00
#define  MHL_VT_PHOTO							0x02
#define  MHL_VT_CINEMA							0x04
#define  MHL_VT_GAMES							0x08
#define  MHL_SUPP_VT							0x80


#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO						(0x01 << 1)
#define	MHL_DEV_LD_AUDIO						(0x01 << 2)
#define	MHL_DEV_LD_MEDIA						(0x01 << 3)
#define	MHL_DEV_LD_TUNER						(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)


#define	MHL_BANDWIDTH_LIMIT					22

#define MHL_STATUS_REG_CONNECTED_RDY        0x30
#define MHL_STATUS_REG_LINK_MODE            0x31

#define  MHL_STATUS_DCAP_RDY				BIT0

#define MHL_STATUS_CLK_MODE_MASK            0x07
#define MHL_STATUS_CLK_MODE_PACKED_PIXEL    0x02
#define MHL_STATUS_CLK_MODE_NORMAL          0x03
#define MHL_STATUS_PATH_EN_MASK             0x08
#define MHL_STATUS_PATH_ENABLED             0x08
#define MHL_STATUS_PATH_DISABLED            0x00
#define MHL_STATUS_MUTED_MASK               0x10

#define MHL_RCHANGE_INT                     0x20
#define MHL_DCHANGE_INT                     0x21

#define  MHL_INT_DCAP_CHG				BIT0
#define  MHL_INT_DSCR_CHG               		BIT1
#define  MHL_INT_REQ_WRT                     	BIT2
#define  MHL_INT_GRT_WRT                     	BIT3


#define	MHL_INT_EDID_CHG				BIT1

#define		MHL_INT_AND_STATUS_SIZE			0x03
#define		MHL_SCRATCHPAD_SIZE				16
#define		MHL_MAX_BUFFER_SIZE				MHL_SCRATCHPAD_SIZE


#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI)

enum {
	MHL_MSC_MSG_RCP = 0x10,
	MHL_MSC_MSG_RCPK = 0x11,
	MHL_MSC_MSG_RCPE = 0x12,
	MHL_MSC_MSG_RAP = 0x20,
	MHL_MSC_MSG_RAPK = 0x21,
};

#define	RCPE_NO_ERROR					0x00
#define	RCPE_INEEFECTIVE_KEY_CODE	0x01
#define	RCPE_BUSY						0x02

enum {
	MHL_ACK = 0x33,
	MHL_NACK = 0x34,
	MHL_ABORT = 0x35,
	MHL_WRITE_STAT = 0x60 | 0x80,
	MHL_SET_INT = 0x60,
	MHL_READ_DEVCAP = 0x61,
	MHL_GET_STATE = 0x62,
	MHL_GET_VENDOR_ID = 0x63,
	MHL_SET_HPD = 0x64,
	MHL_CLR_HPD = 0x65,
	MHL_SET_CAP_ID = 0x66,
	MHL_GET_CAP_ID = 0x67,
	MHL_MSC_MSG = 0x68,
	MHL_GET_SC1_ERRORCODE = 0x69,
	MHL_GET_DDC_ERRORCODE = 0x6A,
	MHL_GET_MSC_ERRORCODE = 0x6B,
	MHL_WRITE_BURST = 0x6C,
	MHL_GET_SC3_ERRORCODE = 0x6D,
};

enum {
	MHL_RCP_CMD_SELECT = 0x00,
	MHL_RCP_CMD_UP = 0x01,
	MHL_RCP_CMD_DOWN = 0x02,
	MHL_RCP_CMD_LEFT = 0x03,
	MHL_RCP_CMD_RIGHT = 0x04,
	MHL_RCP_CMD_RIGHT_UP = 0x05,
	MHL_RCP_CMD_RIGHT_DOWN = 0x06,
	MHL_RCP_CMD_LEFT_UP = 0x07,
	MHL_RCP_CMD_LEFT_DOWN = 0x08,
	MHL_RCP_CMD_ROOT_MENU = 0x09,
	MHL_RCP_CMD_SETUP_MENU = 0x0A,
	MHL_RCP_CMD_CONTENTS_MENU = 0x0B,
	MHL_RCP_CMD_FAVORITE_MENU = 0x0C,
	MHL_RCP_CMD_EXIT = 0x0D,


	MHL_RCP_CMD_NUM_0 = 0x20,
	MHL_RCP_CMD_NUM_1 = 0x21,
	MHL_RCP_CMD_NUM_2 = 0x22,
	MHL_RCP_CMD_NUM_3 = 0x23,
	MHL_RCP_CMD_NUM_4 = 0x24,
	MHL_RCP_CMD_NUM_5 = 0x25,
	MHL_RCP_CMD_NUM_6 = 0x26,
	MHL_RCP_CMD_NUM_7 = 0x27,
	MHL_RCP_CMD_NUM_8 = 0x28,
	MHL_RCP_CMD_NUM_9 = 0x29,

	MHL_RCP_CMD_DOT = 0x2A,
	MHL_RCP_CMD_ENTER = 0x2B,
	MHL_RCP_CMD_CLEAR = 0x2C,


	MHL_RCP_CMD_CH_UP = 0x30,
	MHL_RCP_CMD_CH_DOWN = 0x31,
	MHL_RCP_CMD_PRE_CH = 0x32,
	MHL_RCP_CMD_SOUND_SELECT = 0x33,
	MHL_RCP_CMD_INPUT_SELECT = 0x34,
	MHL_RCP_CMD_SHOW_INFO = 0x35,
	MHL_RCP_CMD_HELP = 0x36,
	MHL_RCP_CMD_PAGE_UP = 0x37,
	MHL_RCP_CMD_PAGE_DOWN = 0x38,


	MHL_RCP_CMD_VOL_UP = 0x41,
	MHL_RCP_CMD_VOL_DOWN = 0x42,
	MHL_RCP_CMD_MUTE = 0x43,
	MHL_RCP_CMD_PLAY = 0x44,
	MHL_RCP_CMD_STOP = 0x45,
	MHL_RCP_CMD_PAUSE = 0x46,
	MHL_RCP_CMD_RECORD = 0x47,
	MHL_RCP_CMD_REWIND = 0x48,
	MHL_RCP_CMD_FAST_FWD = 0x49,
	MHL_RCP_CMD_EJECT = 0x4A,
	MHL_RCP_CMD_FWD = 0x4B,
	MHL_RCP_CMD_BKWD = 0x4C,


	MHL_RCP_CMD_ANGLE = 0x50,
	MHL_RCP_CMD_SUBPICTURE = 0x51,


	MHL_RCP_CMD_PLAY_FUNC = 0x60,
	MHL_RCP_CMD_PAUSE_PLAY_FUNC = 0x61,
	MHL_RCP_CMD_RECORD_FUNC = 0x62,
	MHL_RCP_CMD_PAUSE_REC_FUNC = 0x63,
	MHL_RCP_CMD_STOP_FUNC = 0x64,
	MHL_RCP_CMD_MUTE_FUNC = 0x65,
	MHL_RCP_CMD_UN_MUTE_FUNC = 0x66,
	MHL_RCP_CMD_TUNE_FUNC = 0x67,
	MHL_RCP_CMD_MEDIA_FUNC = 0x68,


	MHL_RCP_CMD_F1 = 0x71,
	MHL_RCP_CMD_F2 = 0x72,
	MHL_RCP_CMD_F3 = 0x73,
	MHL_RCP_CMD_F4 = 0x74,
	MHL_RCP_CMD_F5 = 0x75,

	MHL_RCP_CMD_VS = 0x7E,
	MHL_RCP_CMD_RSVD = 0x7F,
};

#define	MHL_RAP_CONTENT_ON		0x10
#define	MHL_RAP_CONTENT_OFF		0x11

bool_t MhlTxCBusBusy(void);
void MHLSinkOrDonglePowerStatusCheck(void);

#endif
