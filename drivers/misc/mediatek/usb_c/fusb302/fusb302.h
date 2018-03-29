/*********************************************************************
 * FileName:        FUSB300.h
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC32
 * Compiler:        XC32
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * M. Smith         12/04/2014    Initial Version
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the “Company?
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS?CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 ********************************************************************/

#ifndef FUSB300_H
#define	FUSB300_H

    /* /////////////////////////////////////////////////////////////////////////// */
    /* Required headers */
    /* /////////////////////////////////////////////////////////////////////////// */
#define TRUE true
#define FALSE false
#define BOOL bool
#define UINT8 u8
#define UINT16 u16
#define UINT32 u32

#define SDAC_DEFAULT        0x20
#define MDAC_1P6V           0x26
#define MDAC_2P05V          0x31
#define MDAC_2P6V           0x3E

    /* FUSB300 Register Addresses */
#define regDeviceID     0x01
#define regSwitches0    0x02
#define regSwitches1    0x03
#define regMeasure      0x04
#define regSlice        0x05
#define regControl0     0x06
#define regControl1     0x07
#define regControl2     0x08
#define regControl3     0x09
#define regMask         0x0A
#define regPower        0x0B
#define regReset        0x0C
#define regOCPreg       0x0D
#define regMaska        0x0E
#define regMaskb        0x0F
#define regControl4     0x10
#define regStatus0a     0x3C
#define regStatus1a     0x3D
#define regInterrupta   0x3E
#define regInterruptb   0x3F
#define regStatus0      0x40
#define regStatus1      0x41
#define regInterrupt    0x42
#define regFIFO         0x43

    /* Type C Timing Parameters */
#define tAMETimeout     1000
#define tCCDebounceMin  100
#define tCCDebounceNom  120
#define tCCDebounceMax  200
#define tPDDebounceMin  10
#define tPDDebounceMax  20
#define tAccDetect      100
#define tDRP            80
#define tDRPAdvert      30
#define tDRPTransition  1
#define tDRPTry         125
#define tDRPTryWait     600
#define tErrorRecovery  25

/* Max time from entry to Attached.SRC until VBUS reaches minimum vSafe5V */
#define tVBUSOn         275
/* Max time from when the sink is detached until the source removes VBUS and reaches vSafe0V */
#define tVBUSOff        650
/* VConn should be applied prior to VBUS */
#define tVConnOn        2
/* Max time from when Sink enters PoweredAccessory state until sourcing VCONN */
#define tVConnOnPA      100
/* Max time to remove VCONN supply */
#define tVConnOff       35
/* Nominal time for the sink to reduce its consumption due to a change in Type-C current advertisement */
#define tSinkAdj        40
/* Duration in ms to wait before checking other CC pin for the FUSB302 */
#define tFUSB302Toggle  3

typedef enum {
	USBTypeC_Sink = 0,
	USBTypeC_Source,
	USBTypeC_DRP
} USBTypeCPort;

typedef enum {
	Disabled = 0,
	ErrorRecovery,
	Unattached,
	AttachWaitSink,
	AttachedSink,
	AttachWaitSource,
	AttachedSource,
	TrySource,
	TryWaitSink,
	AudioAccessory,
	DebugAccessory,
	AttachWaitAccessory,
	PoweredAccessory,
	UnsupportedAccessory,
	DelayUnattached,
} ConnectionState;

typedef enum {
	CCTypeNone = 0,
	CCTypeRa,
	CCTypeRdUSB,
	CCTypeRd1p5,
	CCTypeRd3p0
} CCTermType;

typedef enum {
	TypeCPin_None = 0,
	TypeCPin_GND1,
	TypeCPin_TXp1,
	TypeCPin_TXn1,
	TypeCPin_VBUS1,
	TypeCPin_CC1,
	TypeCPin_Dp1,
	TypeCPin_Dn1,
	TypeCPin_SBU1,
	TypeCPin_VBUS2,
	TypeCPin_RXn2,
	TypeCPin_RXp2,
	TypeCPin_GND2,
	TypeCPin_GND3,
	TypeCPin_TXp2,
	TypeCPin_TXn2,
	TypeCPin_VBUS3,
	TypeCPin_CC2,
	TypeCPin_Dp2,
	TypeCPin_Dn2,
	TypeCPin_SBU2,
	TypeCPin_VBUS4,
	TypeCPin_RXn1,
	TypeCPin_RXp1,
	TypeCPin_GND4
} TypeCPins_t;

typedef enum {
	utccNone = 0,
	utccDefault,
	utcc1p5A,
	utcc3p0A
} USBTypeCCurrent;

typedef union {
	UINT8 byte;
	struct {
		unsigned REVISION:3;
		unsigned VERSION:5;
	};
} regDeviceID_t;

typedef union {
	UINT16 word;
	UINT8 byte[2] __aligned(8);
	struct {
		/* Switches0 */
		unsigned PDWN1:1;
		unsigned PDWN2:1;
		unsigned MEAS_CC1:1;
		unsigned MEAS_CC2:1;
		unsigned VCONN_CC1:1;
		unsigned VCONN_CC2:1;
		unsigned PU_EN1:1;
		unsigned PU_EN2:1;
		/* Switches1 */
		unsigned TXCC1:1;
		unsigned TXCC2:1;
		unsigned AUTO_CRC:1;
		unsigned:1;
		unsigned DATAROLE:1;
		unsigned SPECREV:2;
		unsigned POWERROLE:1;
	};
} regSwitches_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned MDAC:6;
		unsigned MEAS_VBUS:1;
		unsigned:1;
	};
} regMeasure_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned SDAC:6;
		unsigned:2;
	};
} regSlice_t;

typedef union {
	UINT32 dword;
	UINT8 byte[4] __aligned(8);
	struct {
		/* Control0 */
		unsigned TX_START:1;
		unsigned AUTO_PRE:1;
		unsigned HOST_CUR:2;
		unsigned LOOPBACK:1;
		unsigned INT_MASK:1;
		unsigned TX_FLUSH:1;
		unsigned:1;
		/* Control1 */
		unsigned ENSOP1:1;
		unsigned ENSOP2:1;
		unsigned RX_FLUSH:1;
		unsigned:1;
		unsigned BIST_MODE2:1;
		unsigned ENSOP1DP:1;
		unsigned ENSOP2DB:1;
		unsigned:1;
		/* Control2 */
		unsigned TOGGLE:1;
		unsigned MODE:2;
		unsigned WAKE_EN:1;
		unsigned WAKE_SELF:1;
		unsigned TOG_RD_ONLY:1;
		unsigned:2;
		/* Control3 */
		unsigned AUTO_RETRY:1;
		unsigned N_RETRIES:2;
		unsigned AUTO_SOFTRESET:1;
		unsigned AUTO_HARDRESET:1;
		unsigned:1;
		unsigned SEND_HARDRESET:1;
		unsigned:1;
	};
} regControl_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned M_BC_LVL:1;
		unsigned M_COLLISION:1;
		unsigned M_WAKE:1;
		unsigned M_ALERT:1;
		unsigned M_CRC_CHK:1;
		unsigned M_COMP_CHNG:1;
		unsigned M_ACTIVITY:1;
		unsigned M_VBUSOK:1;
	};
} regMask_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned PWR:4;
		unsigned:4;
	};
} regPower_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned SW_RES:1;
		unsigned:7;
	};
} regReset_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned OCP_CUR:3;
		unsigned OCP_RANGE:1;
		unsigned:4;
	};
} regOCPreg_t;

typedef union {
	UINT16 word;
	UINT8 byte[2] __aligned(8);
	struct {
		/* Maska */
		unsigned M_HARDRST:1;
		unsigned M_SOFTRST:1;
		unsigned M_TXCRCSENT:1;
		unsigned M_HARDSENT:1;
		unsigned M_RETRYFAIL:1;
		unsigned M_SOFTFAIL:1;
		unsigned M_TOGDONE:1;
		unsigned M_OCP_TEMP:1;
		/* Maskb */
		unsigned M_GCRCSENT:1;
		unsigned:7;
	};
} regMaskAdv_t;

typedef union {
	UINT8 byte;
	struct {
		unsigned TOG_USRC_EXIT:1;
		unsigned:7;
	};
} regControl4_t;

typedef union {
	UINT8 byte[7] __aligned(8);
	struct {
		UINT16 StatusAdv;
		UINT16 InterruptAdv;
		UINT16 Status;
		UINT8 Interrupt1;
	};
	struct {
		/* Status0a */
		unsigned HARDRST:1;
		unsigned SOFTRST:1;
		unsigned POWER23:2;
		unsigned RETRYFAIL:1;
		unsigned SOFTFAIL:1;
		unsigned TOGDONE:1;
		unsigned M_OCP_TEMP:1;
		/* Status1a */
		unsigned RXSOP:1;
		unsigned RXSOP1DB:1;
		unsigned RXSOP2DB:1;
		unsigned TOGSS:3;
		unsigned:2;
		/* Interrupta */
		unsigned I_HARDRST:1;
		unsigned I_SOFTRST:1;
		unsigned I_TXSENT:1;
		unsigned I_HARDSENT:1;
		unsigned I_RETRYFAIL:1;
		unsigned I_SOFTFAIL:1;
		unsigned I_TOGDONE:1;
		unsigned I_OCP_TEMP:1;
		/* Interruptb */
		unsigned I_GCRCSENT:1;
		unsigned:7;
		/* Status0 */
		unsigned BC_LVL:2;
		unsigned WAKE:1;
		unsigned ALERT:1;
		unsigned CRC_CHK:1;
		unsigned COMP:1;
		unsigned ACTIVITY:1;
		unsigned VBUSOK:1;
		/* Status1 */
		unsigned OCP:1;
		unsigned OVRTEMP:1;
		unsigned TX_FULL:1;
		unsigned TX_EMPTY:1;
		unsigned RX_FULL:1;
		unsigned RX_EMPTY:1;
		unsigned RXSOP1:1;
		unsigned RXSOP2:1;
		/* Interrupt */
		unsigned I_BC_LVL:1;
		unsigned I_COLLISION:1;
		unsigned I_WAKE:1;
		unsigned I_ALERT:1;
		unsigned I_CRC_CHK:1;
		unsigned I_COMP_CHNG:1;
		unsigned I_ACTIVITY:1;
		unsigned I_VBUSOK:1;
	};
} regStatus_t;

typedef struct {
	regDeviceID_t DeviceID;
	regSwitches_t Switches;
	regMeasure_t Measure;
	regSlice_t Slice;
	regControl_t Control;
	regMask_t Mask;
	regPower_t Power;
	regReset_t Reset;
	regOCPreg_t OCPreg;
	regMaskAdv_t MaskAdv;
	regControl4_t Control4;
	regStatus_t Status;
} FUSB300reg_t;

/* /////////////////////////////////////////////////////////////////////////// */
/* LOCAL PROTOTYPES */
/* /////////////////////////////////////////////////////////////////////////// */
void InitializeFUSB300Interrupt(BOOL blnEnable);
void InitializeFUSB300Timer(BOOL blnEnable);
void InitializeFUSB300Variables(void);
void InitializeFUSB300(void);
void DisableFUSB300StateMachine(void);
void EnableFUSB300StateMachine(void);
void StateMachineFUSB300(struct usbtypc *typec);
void StateMachineDisabled(void);
void StateMachineErrorRecovery(void);
void StateMachineDelayUnattached(void);
void StateMachineUnattached(void);
void StateMachineAttachWaitSnk(void);
void StateMachineAttachWaitSrc(void);
void StateMachineAttachWaitAcc(void);
void StateMachineAttachedSink(void);
void StateMachineAttachedSource(void);
void StateMachineTryWaitSnk(void);
void StateMachineTrySrc(void);
void StateMachineDebugAccessory(void);
void StateMachineAudioAccessory(void);
void StateMachinePoweredAccessory(void);
void StateMachineUnsupportedAccessory(void);
void SetStateDisabled(void);
void SetStateErrorRecovery(void);
void SetStateDelayUnattached(void);
void SetStateUnattached(void);
void SetStateAttachWaitSnk(void);
void SetStateAttachWaitSrc(void);
void SetStateAttachWaitAcc(void);
void SetStateAttachedSrc(void);
void SetStateAttachedSink(void);
void RoleSwapToAttachedSink(void);
void RoleSwapToAttachedSource(void);
void SetStateTryWaitSnk(void);
void SetStateTrySrc(void);
void SetStateDebugAccessory(void);
void SetStateAudioAccessory(void);
void SetStatePoweredAccessory(void);
void SetStateUnsupportedAccessory(void);
void UpdateSourcePowerMode(void);
void ToggleMeasureCC1(void);
void ToggleMeasureCC2(void);
CCTermType DecodeCCTermination(void);
void UpdateSinkCurrent(CCTermType Termination);
void ConfigurePortType(unsigned char Control);
void UpdateCurrentAdvert(unsigned char Current);
void GetFUSB300TypeCStatus(unsigned char abytData[]);
unsigned char GetTypeCSMControl(void);
unsigned char GetCCTermination(void);

BOOL FUSB300Write(unsigned char regAddr, unsigned char length, unsigned char *data);
BOOL FUSB300Read(unsigned char regAddr, unsigned char length, unsigned char *data);

void fusb300_i2c_w_reg8(struct i2c_client *client, u8 addr, u8 var);
u8 fusb300_i2c_r_reg(struct i2c_client *client, u8 addr);
#endif	/* FUSB300_H */
