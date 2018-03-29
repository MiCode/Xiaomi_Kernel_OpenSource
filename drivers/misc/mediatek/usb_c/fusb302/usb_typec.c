/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <typec.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>

#include "fusb302.h"

#ifndef USB_TYPE_C
#define USB_TYPE_C

#define K_EMERG	(1<<7)
#define K_QMU	(1<<7)
#define K_ALET		(1<<6)
#define K_CRIT		(1<<5)
#define K_ERR		(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

#define fusb_printk(level, fmt, args...) do { \
		if (debug_level & level) { \
			pr_err("[FUSB302]" fmt, ## args); \
		} \
	} while (0)

#define SKIP_TIMER

static u32 debug_level = (255 - K_DEBUG);
static struct usbtypc *g_exttypec;
static struct i2c_client *typec_client;
static int trigger_driver(struct usbtypc *typec, int type, int stat, int dir);
const char string_conection_state[18][32] = {
	"Disabled",
	"ErrorRecovery",
	"Unattached",
	"AttachWaitSink",
	"AttachedSink",
	"AttachWaitSource",
	"AttachedSource",
	"TrySource",
	"TryWaitSink",
	"AudioAccessory",
	"DebugAccessory",
	"AttachWaitAccessory",
	"PoweredAccessory",
	"UnsupportedAccessory",
	"DelayUnattached",
};


/* /////////////////////////////////////////////////////////////////////////// */
/* Variables accessible outside of the FUSB300 state machine */
/* /////////////////////////////////////////////////////////////////////////// */
static FUSB300reg_t Registers;	/* Variable holding the current status of the FUSB300 registers */
static BOOL USBPDActive;	/* Variable to indicate whether the USB PD state machine is active or not */
static BOOL USBPDEnabled;	/* Variable to indicate whether USB PD is enabled (by the host) */
static UINT32 PRSwapTimer;	/* Timer used to bail out of a PR_Swap from the Type-C side if necessary */

static USBTypeCPort PortType;	/* Variable indicating which type of port we are implementing */
static BOOL blnCCPinIsCC1;	/* Flag to indicate if the CC1 pin has been detected as the CC pin */
static BOOL blnCCPinIsCC2;	/* Flag to indicate if the CC2 pin has been detected as the CC pin */
static BOOL blnSMEnabled;	/* Flag to indicate whether the FUSB300 state machine is enabled */
static ConnectionState ConnState;	/* Variable indicating the current connection state */

/* /////////////////////////////////////////////////////////////////////////// */
/* Variables accessible only inside FUSB300 state machine */
/* /////////////////////////////////////////////////////////////////////////// */
static BOOL blnSrcPreferred;	/* Flag to indicate whether we prefer the Src role when in DRP */
static BOOL blnAccSupport;	/* Flag to indicate whether the port supports accessories */
static BOOL blnINTActive;	/* Flag to indicate that an interrupt occurred that needs to be handled */
static UINT16 StateTimer;	/* Timer used to validate proceeding to next state */
static UINT16 DebounceTimer1;	/* Timer used for first level debouncing */
static UINT16 DebounceTimer2;	/* Timer used for second level debouncing */
static UINT16 ToggleTimer;	/* Timer used for CC swapping in the FUSB302 */
static CCTermType CC1TermAct;	/* Active CC1 termination value */
static CCTermType CC2TermAct;	/* Active CC2 termination value */
static CCTermType CC1TermDeb;	/* Debounced CC1 termination value */
static CCTermType CC2TermDeb;	/* Debounced CC2 termination value */
static USBTypeCCurrent SinkCurrent;	/* Variable to indicate the current capability we have received */
static USBTypeCCurrent SourceCurrent;	/* Variable to indicate the current capability we are broadcasting */

/*******************************************************************************
 * Function:        InitializeFUSB300Variables
 * Input:           None
 * Return:          None
 * Description:     Initializes the FUSB300 state machine variables
 ******************************************************************************/
void InitializeFUSB300Variables(void)
{
	blnSMEnabled = TRUE;	/* Disable the FUSB300 state machine by default */
	blnAccSupport = FALSE;	/* Disable accessory support by default */
	blnSrcPreferred = FALSE;	/* Clear the source preferred flag by default */
	PortType = USBTypeC_DRP;	/* Initialize to a dual-role port by default */
	ConnState = Disabled;	/* Initialize to the disabled state? */
	blnINTActive = FALSE;	/* Clear the handle interrupt flag */
	blnCCPinIsCC1 = FALSE;	/* Clear the flag to indicate CC1 is CC */
	blnCCPinIsCC2 = FALSE;	/* Clear the flag to indicate CC2 is CC */
	StateTimer = USHRT_MAX;	/* Disable the state timer */
	DebounceTimer1 = USHRT_MAX;	/* Disable the 1st debounce timer */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd debounce timer */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer */
	CC1TermDeb = CCTypeNone;	/* Set the CC1 termination type to none initially */
	CC2TermDeb = CCTypeNone;	/* Set the CC2 termination type to none initially */
	CC1TermAct = CC1TermDeb;	/* Initialize the active CC1 value */
	CC2TermAct = CC2TermDeb;	/* Initialize the active CC2 value */
	SinkCurrent = utccNone;	/* Clear the current advertisement initially */
	SourceCurrent = utccDefault;	/* Set the current advertisement to the default */
	Registers.DeviceID.byte = 0x00;	/* Clear */
	Registers.Switches.byte[0] = 0x03;	/* Only enable the device pull-downs by default */
	Registers.Switches.byte[1] = 0x00;	/* Disable the BMC transmit drivers */
	Registers.Measure.byte = 0x00;	/* Clear */
	Registers.Slice.byte = SDAC_DEFAULT;	/* Set the SDAC threshold to ~0.544V by default (from FUSB302) */
	Registers.Control.byte[0] = 0x20;	/* Set to mask all interrupts by default (from FUSB302) */
	Registers.Control.byte[1] = 0x00;	/* Clear */
	Registers.Control.byte[2] = 0x02;	/*  */
	Registers.Control.byte[3] = 0x06;	/*  */
	Registers.Mask.byte = 0x00;	/* Clear */
	Registers.Power.byte = 0x07;	/* Initialize to everything enabled except oscillator */
	Registers.Status.Status = 0;	/* Clear status bytes */
	Registers.Status.StatusAdv = 0;	/* Clear the advanced status bytes */
	Registers.Status.Interrupt1 = 0;	/* Clear the interrupt1 byte */
	Registers.Status.InterruptAdv = 0;	/* Clear the advanced interrupt bytes */
	USBPDActive = FALSE;	/* Clear the USB PD active flag */
	USBPDEnabled = TRUE;	/* Clear the USB PD enabled flag until enabled by the host */
	PRSwapTimer = 0;	/* Clear the PR Swap timer */
}

void InitializeFUSB300(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	FUSB300Read(regDeviceID, 1, &Registers.DeviceID.byte);	/* Read the device ID */
	FUSB300Read(regSlice, 1, &Registers.Slice.byte);	/* Read the slice */
	Registers.Mask.byte = 0x00;	/* Do not mask any interrupts */

	/* Clear all interrupt masks (we want to do something with them) */
	FUSB300Write(regMask, 1, &Registers.Mask.byte);
	/* Update the control and power values since they will be written
	 * in either the Unattached.UFP or Unattached.DFP states */
	Registers.Control.dword = 0x06220004;	/* Reset all back to default, but clear the INT_MASK bit */

	switch (PortType) {
	case USBTypeC_Sink:
		Registers.Control.MODE = 0x2;
		break;
	case USBTypeC_Source:
		Registers.Control.MODE = 0x3;
		break;
	default:
		Registers.Control.MODE = 0x1;
		break;
	}

	/* Update the control registers for toggling */
	FUSB300Write(regControl2, 2, &Registers.Control.byte[2]);
	Registers.Control4.TOG_USRC_EXIT = 1;	/* Stop toggling with Ra/Ra */
	FUSB300Write(regControl4, 1, &Registers.Control4.byte);	/* Commit to the device */
	/* Initialize such that only the bandgap and wake circuit are enabled by default */
	Registers.Power.byte = 0x01;
	/* Read the advanced status registers to make sure we are in sync with the device */
	FUSB300Read(regStatus0a, 2, &Registers.Status.byte[0]);
	/* Read the standard status registers to make sure we are in sync with the device */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);
	/* Do not read any of the interrupt registers, let the state machine handle those */
	/* SetStateDelayUnattached(); */
	SetStateUnattached();
}

void DisableFUSB300StateMachine(void)
{
	blnSMEnabled = FALSE;
	SetStateDisabled();
}

void EnableFUSB300StateMachine(void)
{
	InitializeFUSB300();
	blnSMEnabled = TRUE;
}


/*******************************************************************************
 * Function:        StateMachineFUSB300
 * Input:           None
 * Return:          None
 * Description:     This is the state machine for the entire USB PD
 *                  This handles all the communication between the master and
 *                  slave.  This function is called by the Timer3 ISR on a
 *                  sub-interval of the 1/4 UI in order to meet timing
 *                  requirements.
 ******************************************************************************/
void StateMachineFUSB300(struct usbtypc *typec)
{
	ConnectionState pre_ConnState = ConnState;
	ConnectionState new_ConnState;

	if (!blnSMEnabled)
		return;

	fusb_printk(K_DEBUG, "StateMachineFUSB300+ ConnState=%s\n",
		    string_conection_state[ConnState]);


	FUSB300Read(regInterrupt, 1, &Registers.Status.Interrupt1);	/* Read the interrupt */
	FUSB300Read(regStatus0, 2, (unsigned char *)&Registers.Status.Status);	/* Read the status */
	FUSB300Read(regInterrupta, 2, (unsigned char *)&Registers.Status.InterruptAdv);	/* Read the status */
	FUSB300Read(regStatus0a, 2, (unsigned char *)&Registers.Status.StatusAdv);	/* Read the status */
	fusb_printk(K_DEBUG, "StatusAdv==0x%x\n", Registers.Status.StatusAdv);
	fusb_printk(K_DEBUG, "InterruptAdv==0x%x\n", Registers.Status.InterruptAdv);
	fusb_printk(K_DEBUG, "Status==0x%x\n", Registers.Status.Status);
	fusb_printk(K_DEBUG, "Interrupt1==0x%x\n", Registers.Status.Interrupt1);

#if 0
	/* Only call the USB PD routines if we have enabled the block */
	if (USBPDActive) {
		/* Call the protocol state machine to handle any timing critical operations */
		USBPDProtocol();
		/* Once we have handled any Type-C and protocol events, call the USB PD Policy Engine */
		USBPDPolicyEngine();
	}
#endif

	switch (ConnState) {
	case Disabled:
		StateMachineDisabled();
		break;
	case ErrorRecovery:
		StateMachineErrorRecovery();
		break;
	case Unattached:
		StateMachineUnattached();
		break;
	case AttachWaitSink:
		StateMachineAttachWaitSnk();
		break;
	case AttachedSink:
		StateMachineAttachedSink();
		break;
	case AttachWaitSource:
		StateMachineAttachWaitSrc();
		break;
	case AttachedSource:
		StateMachineAttachedSource();
		break;
	case TrySource:
		StateMachineTrySrc();
		break;
	case TryWaitSink:
		StateMachineTryWaitSnk();
		break;
	case AudioAccessory:
		StateMachineAudioAccessory();
		break;
	case DebugAccessory:
		StateMachineDebugAccessory();
		break;
	case AttachWaitAccessory:
		StateMachineAttachWaitAcc();
		break;
	case PoweredAccessory:
		StateMachinePoweredAccessory();
		break;
	case UnsupportedAccessory:
		StateMachineUnsupportedAccessory();
		break;
	case DelayUnattached:
		StateMachineDelayUnattached();
		break;
	default:
		/* We shouldn't get here, so go to the unattached state just in case */
		SetStateDelayUnattached();
		break;
	}
	/* Clear the interrupt register once we've gone through the state machines */
	Registers.Status.Interrupt1 = 0;
	/* Clear the advanced interrupt registers once we've gone through the state machines */
	Registers.Status.InterruptAdv = 0;

	new_ConnState = ConnState;

	fusb_printk(K_INFO, "StateMachineFUSB300- ConnState=%s -> %s\n",
		string_conection_state[pre_ConnState],
		string_conection_state[new_ConnState]);

#ifdef SKIP_TIMER
	if (new_ConnState != pre_ConnState)
		StateMachineFUSB300(typec);
#endif
	if (!typec->en_irq) {
		fusb_printk(K_DEBUG, "Enable IRQ\n");
		typec->en_irq = 1;
		enable_irq(typec->irqnum);
	}
}



void StateMachineDisabled(void)
{
	/* Do nothing until directed to go to some other state... */
}

void StateMachineErrorRecovery(void)
{
	if (StateTimer == 0)
		SetStateDelayUnattached();
}

void StateMachineDelayUnattached(void)
{
	if (StateTimer == 0)
		SetStateUnattached();
}

void StateMachineUnattached(void)
{
	if (Registers.Status.I_TOGDONE) {
		switch (Registers.Status.TOGSS) {
		case 0x05:	/* Rp detected on CC1 */
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = FALSE;
			SetStateAttachWaitSnk();	/* Go to the AttachWaitSnk state */
			break;
		case 0x06:	/* Rp detected on CC2 */
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = TRUE;
			SetStateAttachWaitSnk();	/* Go to the AttachWaitSnk state */
			break;
		case 0x01:	/* Rd detected on CC1 */
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = false;
			/* If we are configured as a sink and support accessories... */
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))
				SetStateAttachWaitAcc();	/* Go to the AttachWaitAcc state */
			else	/* Otherwise we must be configured as a source or DRP */
				SetStateAttachWaitSrc();	/* So go to the AttachWaitSnk state */
			break;
		case 0x02:	/* Rd detected on CC2 */
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = true;
			/* If we are configured as a sink and support accessories... */
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))
				SetStateAttachWaitAcc();	/* Go to the AttachWaitAcc state */
			else	/* Otherwise we must be configured as a source or DRP */
				SetStateAttachWaitSrc();	/* So go to the AttachWaitSnk state */
			break;
		case 0x07:	/* Ra detected on both CC1 and CC2 */
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = false;
			/* If we are configured as a sink and support accessories... */
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))
				SetStateAttachWaitAcc();	/* Go to the AttachWaitAcc state */
			else	/* Otherwise we must be configured as a source or DRP */
				SetStateAttachWaitSrc();	/* So go to the AttachWaitSnk state */
			break;
		default:	/* Shouldn't get here, but just in case reset everything... */
			Registers.Control.TOGGLE = 0;	/* Disable the toggle in order to clear... */
			/* Commit the control state */
			FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);
			udelay(10);
			/* Re-enable the toggle state machine... (allows us to get another I_TOGDONE interrupt) */
			Registers.Control.TOGGLE = 1;
			/* Commit the control state */
			FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);
			break;
		}
	}
	/* rand(); */
}

void StateMachineAttachWaitSnk(void)
{
	/* If the both CC lines has been open for tPDDebounce, go to the unattached state */
	/* If VBUS and the we've been Rd on exactly one pin for 100ms... go to the attachsnk state */
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s, CCValue: %d\n", __func__, CCValue);
	if (Registers.Switches.MEAS_CC1) {	/* If we are looking at CC1 */
		if (CC1TermAct != CCValue) {
			/* Check to see if the value has changed... */
			CC1TermAct = CCValue;	/* If it has, update the value */
			/* Restart the debounce timer with tPDDebounce (wait 10ms before detach) */
			DebounceTimer1 = tPDDebounceMin;
		}
	} else {			/* Otherwise we are looking at CC2 */
		if (CC2TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC2TermAct = CCValue;	/* If it has, update the value */
			/* Restart the debounce timer with tPDDebounce (wait 10ms before detach) */
			DebounceTimer1 = tPDDebounceMin;
		}
	}
	fusb_printk(K_DEBUG, "%s, CC1TermAct: %d, CC2TermAct: %d\n", __func__, CC1TermAct,
		    CC2TermAct);
#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
#endif
	if (DebounceTimer1 == 0) {	/* Check to see if our debounce timer has expired... */
		/* If it has, disable it so we don't come back in here until we have debounced a change in state */
		DebounceTimer1 = USHRT_MAX;
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			/* Once the CC state is known, start the tCCDebounce timer to validate */
			DebounceTimer2 = tCCDebounceMin - tPDDebounceMin;
		}
		CC1TermDeb = CC1TermAct;	/* Update the CC1 debounced value */
		CC2TermDeb = CC2TermAct;	/* Update the CC2 debounced value */
	}

	fusb_printk(K_DEBUG, "%s, CC1TermDeb: %d, CC2TermDeb: %d\n", __func__, CC1TermDeb, CC2TermDeb);
#ifdef SKIP_TIMER
	ToggleTimer = 0;
#endif
	if (ToggleTimer == 0) {
		/* If are toggle timer has expired, it's time to swap detection */
		if (Registers.Switches.MEAS_CC1)	/* If we are currently on the CC1 pin... */
			ToggleMeasureCC2();	/* Toggle over to look at CC2 */
		else		/* Otherwise assume we are using the CC2... */
			ToggleMeasureCC1();	/* So toggle over to look at CC1 */

		/* Reset the toggle timer to our default toggling
		 * (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		 */
		ToggleTimer = tFUSB302Toggle;
	}
#ifdef SKIP_TIMER
	DebounceTimer2 = 0;
#endif
	/* If we have detected SNK.Open for atleast tPDDebounce on both pins... */
	if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))
		SetStateDelayUnattached();	/* Go to the unattached state */
	else if (Registers.Status.VBUSOK && (DebounceTimer2 == 0)) {
		/* If we have detected VBUS and we have detected an Rp for >tCCDebounce... */
		fusb_printk(K_DEBUG, "%s__%d, %d, %d\n", __func__, CC1TermDeb, CC2TermDeb, CCTypeRa);
		if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb <= CCTypeRa)) { /* If Rp is detected on CC1 */
			/* If we are configured as a DRP and prefer the source role... */
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)
				SetStateTrySrc();	/* Go to the Try.Src state */
			else {	/* Otherwise we are free to attach as a sink */
				trigger_driver(g_exttypec, DEVICE_TYPE, ENABLE, UP_SIDE);
				blnCCPinIsCC1 = TRUE;	/* Set the CC pin to CC1 */
				blnCCPinIsCC2 = FALSE;	/*  */
				SetStateAttachedSink();	/* Go to the Attached.Snk state */
			}
		} else if ((CC1TermDeb <= CCTypeRa) && (CC2TermDeb > CCTypeRa)) {
			/* If Rp is detected on CC2 */
			/* If we are configured as a DRP and prefer the source role... */
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)
				SetStateTrySrc();	/* Go to the Try.Src state */
			else {	/* Otherwise we are free to attach as a sink */
				trigger_driver(g_exttypec, DEVICE_TYPE, ENABLE, DOWN_SIDE);
				blnCCPinIsCC1 = FALSE;	/*  */
				blnCCPinIsCC2 = TRUE;	/* Set the CC pin to CC2 */
				SetStateAttachedSink();	/* Go to the Attached.Snk State */
			}
		}
	}
}

void StateMachineAttachWaitSrc(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

	if (Registers.Switches.MEAS_CC1) {	/* If we are looking at CC1 */
		if (CC1TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC1TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tPDDebounceMin;	/* Restart the debounce timer (tPDDebounce) */
		}
	} else {			/* Otherwise we are looking at CC2 */
		if (CC2TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC2TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tPDDebounceMin;	/* Restart the debounce timer (tPDDebounce) */
		}
	}
#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
#endif
	if (DebounceTimer1 == 0) {	/* Check to see if our debounce timer has expired... */
		/* If it has, disable it so we don't come back in here until we have debounced a change in state */
		DebounceTimer1 = USHRT_MAX;
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			/* Once the CC state is known, start the tCCDebounce timer to validate */
			DebounceTimer2 = tCCDebounceMin;
		}
		CC1TermDeb = CC1TermAct;	/* Update the CC1 debounced value */
		CC2TermDeb = CC2TermAct;	/* Update the CC2 debounced value */
	}
#ifdef SKIP_TIMER
	ToggleTimer = 0;
#endif
	if (ToggleTimer == 0) {	/* If are toggle timer has expired, it's time to swap detection */
		if (Registers.Switches.MEAS_CC1)	/* If we are currently on the CC1 pin... */
			ToggleMeasureCC2();	/* Toggle over to look at CC2 */
		else		/* Otherwise assume we are using the CC2... */
			ToggleMeasureCC1();	/* So toggle over to look at CC1 */

		/* Reset the toggle timer to our default toggling
		 * (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		 */
		ToggleTimer = tFUSB302Toggle;
	}
#ifdef SKIP_TIMER
	DebounceTimer2 = 0;
#endif
	if ((CC1TermDeb == CCTypeNone) && (CC2TermDeb == CCTypeNone)) {
		/* If our debounced signals are both open, go to the unattached state */
		SetStateDelayUnattached();
	} else if ((CC1TermDeb == CCTypeNone) && (CC2TermDeb == CCTypeRa)) {
		/* If exactly one pin is open and the other is Ra, go to the unattached state */
		SetStateDelayUnattached();
	} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeNone)) {
		/* If exactly one pin is open and the other is Ra, go to the unattached state */
		SetStateDelayUnattached();
	} else if (DebounceTimer2 == 0)	{
		/* Otherwise, we are checking to see if we have had a solid state for tCCDebounce */
		if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))
			SetStateAudioAccessory(); /* If both pins are Ra, it's an audio accessory */
		else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb > CCTypeRa))
			SetStateDebugAccessory(); /* If both pins are Rd, it's a debug accessory */
		else if (CC1TermDeb > CCTypeRa)	{
			/* If CC1 is Rd and CC2 is not... */
			trigger_driver(g_exttypec, HOST_TYPE, ENABLE, UP_SIDE);
			blnCCPinIsCC1 = TRUE;	/* Set the CC pin to CC1 */
			blnCCPinIsCC2 = FALSE;
			SetStateAttachedSrc();	/* Go to the Attached.Src state */
		} else if (CC2TermDeb > CCTypeRa) {
			/* If CC2 is Rd and CC1 is not... */
			trigger_driver(g_exttypec, HOST_TYPE, ENABLE, DOWN_SIDE);
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = TRUE;	/* Set the CC pin to CC2 */
			SetStateAttachedSrc();	/* Go to the Attached.Src state */
		}
	}
}

void StateMachineAttachWaitAcc(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

	if (Registers.Switches.MEAS_CC1) {
		/* If we are looking at CC1 */
		if (CC1TermAct != CCValue) {
			/* Check to see if the value has changed... */
			CC1TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tCCDebounceNom;	/* Restart the debounce timer (tCCDebounce) */
		}
	} else {		/* Otherwise we are looking at CC2 */
		if (CC2TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC2TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tCCDebounceNom;	/* Restart the debounce timer (tCCDebounce) */
		}
	}
	if (ToggleTimer == 0) {	/* If are toggle timer has expired, it's time to swap detection */
		if (Registers.Switches.MEAS_CC1)	/* If we are currently on the CC1 pin... */
			ToggleMeasureCC2();	/* Toggle over to look at CC2 */
		else		/* Otherwise assume we are using the CC2... */
			ToggleMeasureCC1();	/* So toggle over to look at CC1 */
		/* Reset the toggle timer to our default toggling (<tPDDebounce to avoid
		 * disconnecting the other side when we remove pull-ups) */
		ToggleTimer = tFUSB302Toggle;
	}
#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (DebounceTimer1 == 0) {	/* Check to see if the signals have been stable for tCCDebounce */
		if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa)) {
			/* If they are both Ra, it's an audio accessory */
			SetStateAudioAccessory();
		} else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb > CCTypeRa)) {
			/* If they are both Rd, it's a debug accessory */
			SetStateDebugAccessory();
		} else if ((CC1TermDeb == CCTypeNone) || (CC2TermDeb == CCTypeNone)) {
			/* If either pin is open, it's considered a detach */
			SetStateDelayUnattached();
		} else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb == CCTypeRa)) {
			/* If CC1 is Rd and CC2 is Ra, it's a powered accessory (CC1 is CC) */
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = FALSE;
			SetStatePoweredAccessory();
		} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb > CCTypeRa)) {
			/* If CC1 is Ra and CC2 is Rd, it's a powered accessory (CC2 is CC) */
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = FALSE;
			SetStatePoweredAccessory();
		}
	}
}

void StateMachineAttachedSink(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s, CCTermType=0x%x\n", __func__, CCValue);

#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	ToggleTimer = 0;
#endif

	if ((Registers.Status.VBUSOK == FALSE) && (!PRSwapTimer)) {
		/* If VBUS is removed and we are not in the middle of a power role swap... */
		SetStateDelayUnattached();	/* Go to the unattached state */
		trigger_driver(g_exttypec, DEVICE_TYPE, DISABLE, DONT_CARE);
	} else {
		if (Registers.Switches.MEAS_CC1) {	/* If we are looking at CC1 */
			if (CCValue != CC1TermAct) {	/* If the CC voltage has changed... */
				CC1TermAct = CCValue;	/* Store the updated value */
				/* Reset the debounce timer to the minimum tPDdebounce */
				DebounceTimer1 = tPDDebounceMin;
			} else if (DebounceTimer1 == 0)	{ /* If the signal has been debounced */
				DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
				CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
				UpdateSinkCurrent(CC1TermDeb);	/* Update the advertised current */
			}
		} else {
			if (CCValue != CC2TermAct) {	/* If the CC voltage has changed... */
				CC2TermAct = CCValue;	/* Store the updated value */
				/* Reset the debounce timer to the minimum tPDdebounce */
				DebounceTimer1 = tPDDebounceMin;
			} else if (DebounceTimer1 == 0) {	/* If the signal has been debounced */
				DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
				CC2TermDeb = CC2TermAct;	/* Store the debounced termination for CC2 */
				UpdateSinkCurrent(CC2TermDeb);	/* Update the advertised current */
			}
		}
	}
}

void StateMachineAttachedSource(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s, CCTermType=0x%x\n", __func__, CCValue);

	if (Registers.Switches.MEAS_CC1) {	/* Did we detect CC1 as the CC pin? */
		if (CC1TermAct != CCValue) {	/* If the CC voltage has changed... */
			CC1TermAct = CCValue;	/* Store the updated value */
			/* Reset the debounce timer to the minimum tPDdebounce */
			DebounceTimer1 = tPDDebounceMin;
		} else if (DebounceTimer1 == 0) { /* If the signal has been debounced */
			DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
			CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
		}
#ifdef SKIP_TIMER
		PRSwapTimer = 0;
		CC1TermDeb = CCValue;
		CC1TermAct = CCValue;
#endif
		if ((CC1TermDeb == CCTypeNone) && (!PRSwapTimer)) {
			/* If the debounced CC pin is detected as open and we aren't in the middle of a PR_Swap */
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred) {
				/* Check to see if we need to go to the TryWait.SNK state... */
				SetStateTryWaitSnk();
			} else {	/* Otherwise we are going to the unattached state */
				SetStateDelayUnattached();
				trigger_driver(g_exttypec, HOST_TYPE, DISABLE, DONT_CARE);
			}
		}
	} else { /* We must have detected CC2 as the CC pin */
		if (CC2TermAct != CCValue) {	/* If the CC voltage has changed... */
			CC2TermAct = CCValue;	/* Store the updated value */
			/* Reset the debounce timer to the minimum tPDdebounce */
			DebounceTimer1 = tPDDebounceMin;
		} else if (DebounceTimer1 == 0) {	/* If the signal has been debounced */
			DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
			CC2TermDeb = CC2TermAct;	/* Store the debounced termination for CC1 */
		}
#ifdef SKIP_TIMER
		PRSwapTimer = 0;
		CC2TermDeb = CCValue;
		CC2TermAct = CCValue;
#endif
		if ((CC2TermDeb == CCTypeNone) && (!PRSwapTimer)) {
			/* If the debounced CC pin is detected as open and we aren't in the middle of a PR_Swap */
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred) {
				/* Check to see if we need to go to the TryWait.SNK state... */
				SetStateTryWaitSnk();
			} else {	/* Otherwise we are going to the unattached state */
				SetStateDelayUnattached();
				trigger_driver(g_exttypec, HOST_TYPE, DISABLE, DONT_CARE);
			}
		}
	}
}

void StateMachineTryWaitSnk(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

	if (Registers.Switches.MEAS_CC1) {	/* If we are looking at CC1 */
		if (CC1TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC1TermAct = CCValue;	/* If it has, update the value */
			/* Restart the debounce timer with tPDDebounce (wait 10ms before detach) */
			DebounceTimer1 = tPDDebounceMin;
		}
	} else {	/* Otherwise we are looking at CC2 */
		if (CC2TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC2TermAct = CCValue;	/* If it has, update the value */
			/* Restart the debounce timer with tPDDebounce (wait 10ms before detach) */
			DebounceTimer1 = tPDDebounceMin;
		}
	}
#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
#endif
	if (DebounceTimer1 == 0) {	/* Check to see if our debounce timer has expired... */
		/* If it has, disable it so we don't come back in here until we have debounced a change in state */
		DebounceTimer1 = USHRT_MAX;
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			/* Once the CC state is known, start the tCCDebounce timer to validate */
			DebounceTimer2 = tCCDebounceMin - tPDDebounceMin;
		}
		CC1TermDeb = CC1TermAct;	/* Update the CC1 debounced value */
		CC2TermDeb = CC2TermAct;	/* Update the CC2 debounced value */
	}
#ifdef SKIP_TIMER
	ToggleTimer = 0;
#endif
	if (ToggleTimer == 0) {		/* If are toggle timer has expired, it's time to swap detection */
		if (Registers.Switches.MEAS_CC1)	/* If we are currently on the CC1 pin... */
			ToggleMeasureCC2();	/* Toggle over to look at CC2 */
		else		/* Otherwise assume we are using the CC2... */
			ToggleMeasureCC1();	/* So toggle over to look at CC1 */
		/* Reset the toggle timer to our default toggling (<tPDDebounce to
		 * avoid disconnecting the other side when we remove pull-ups)
		 */
		ToggleTimer = tFUSB302Toggle;
	}
#ifdef SKIP_TIMER
	DebounceTimer2 = 0;
#endif
	/* If tDRPTryWait has expired and we detected open on both pins... */
	if ((StateTimer == 0) && (CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))
		SetStateDelayUnattached();	/* Go to the unattached state */
	else if (Registers.Status.VBUSOK && (DebounceTimer2 == 0)) {
		/* If we have detected VBUS and we have detected an Rp for >tCCDebounce... */
		if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb == CCTypeRa)) {	/* If Rp is detected on CC1 */
			blnCCPinIsCC1 = TRUE;	/* Set the CC pin to CC1 */
			blnCCPinIsCC2 = FALSE;	/*  */
			SetStateAttachedSink();	/* Go to the Attached.Snk state */
		} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb > CCTypeRa)) {
			/* If Rp is detected on CC2 */
			blnCCPinIsCC1 = FALSE;	/*  */
			blnCCPinIsCC2 = TRUE;	/* Set the CC pin to CC2 */
			SetStateAttachedSink();	/* Go to the Attached.Snk State */
		}
	}
}

void StateMachineTrySrc(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

	if (Registers.Switches.MEAS_CC1) {	/* If we are looking at CC1 */
		if (CC1TermAct != CCValue) {	/* Check to see if the value has changed... */
			CC1TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tPDDebounceMin;	/* Restart the debounce timer (tPDDebounce) */
		}
	} else { /* Otherwise we are looking at CC2 */
		if (CC2TermAct != CCValue) {
			/* Check to see if the value has changed... */
			CC2TermAct = CCValue;	/* If it has, update the value */
			DebounceTimer1 = tPDDebounceMin;	/* Restart the debounce timer (tPDDebounce) */
		}
	}
#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (DebounceTimer1 == 0) {	/* Check to see if our debounce timer has expired... */
		/* If it has, disable it so we don't come back in here until a new debounce value is ready */
		DebounceTimer1 = USHRT_MAX;
		CC1TermDeb = CC1TermAct;	/* Update the CC1 debounced value */
		CC2TermDeb = CC2TermAct;	/* Update the CC2 debounced value */
	}
	if (ToggleTimer == 0) {	/* If are toggle timer has expired, it's time to swap detection */
		if (Registers.Switches.MEAS_CC1)	/* If we are currently on the CC1 pin... */
			ToggleMeasureCC2();	/* Toggle over to look at CC2 */
		else	/* Otherwise assume we are using the CC2... */
			ToggleMeasureCC1();	/* So toggle over to look at CC1 */
		/* Reset the toggle timer to the max tPDDebounce to ensure the other side sees the
		 * pull-up for the min tPDDebounce */
		ToggleTimer = tPDDebounceMax;
	}
	if ((CC1TermDeb > CCTypeRa) && ((CC2TermDeb == CCTypeNone) || (CC2TermDeb == CCTypeRa))) {
		/* If the CC1 pin is Rd for atleast tPDDebounce... */
		blnCCPinIsCC1 = TRUE;	/* The CC pin is CC1 */
		blnCCPinIsCC2 = FALSE;
		SetStateAttachedSrc();	/* Go to the Attached.Src state */
	} else if ((CC2TermDeb > CCTypeRa) && ((CC1TermDeb == CCTypeNone) || (CC1TermDeb == CCTypeRa)))	{
		/* If the CC2 pin is Rd for atleast tPDDebounce... */
		blnCCPinIsCC1 = FALSE;	/* The CC pin is CC2 */
		blnCCPinIsCC2 = TRUE;
		SetStateAttachedSrc();	/* Go to the Attached.Src state */
	} else if (StateTimer == 0) {
		/* If we haven't detected Rd on exactly one of the pins and we have waited for tDRPTry... */
		SetStateTryWaitSnk();	/* Move onto the TryWait.Snk state to not get stuck in here */
	}
}

void StateMachineDebugAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (CC1TermAct != CCValue) {	/* If the CC voltage has changed... */
		CC1TermAct = CCValue;	/* Store the updated value */
		DebounceTimer1 = tCCDebounceMin;	/* Reset the debounce timer to the minimum tCCDebounce */
	} else if (DebounceTimer1 == 0) {	/* If the signal has been debounced */
		DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
		CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
	}
	if (CC1TermDeb == CCTypeNone)	/* If we have detected an open for > tCCDebounce */
		SetStateDelayUnattached();	/* Go to the unattached state */

}

void StateMachineAudioAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (CC1TermAct != CCValue) {	/* If the CC voltage has changed... */
		CC1TermAct = CCValue;	/* Store the updated value */
		DebounceTimer1 = tCCDebounceMin;	/* Reset the debounce timer to the minimum tCCDebounce */
	} else if (DebounceTimer1 == 0) {	/* If the signal has been debounced */
		DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
		CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
	}
	if (CC1TermDeb == CCTypeNone)	/* If we have detected an open for > tCCDebounce */
		SetStateDelayUnattached();	/* Go to the unattached state */
}

void StateMachinePoweredAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (CC1TermAct != CCValue) {	/* If the CC voltage has changed... */
		CC1TermAct = CCValue;	/* Store the updated value */
		DebounceTimer1 = tPDDebounceMin;	/* Reset the debounce timer to the minimum tPDdebounce */
	} else if (DebounceTimer1 == 0) {/* If the signal has been debounced */
		DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
		CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
	}
	if (CC1TermDeb == CCTypeNone)	/* If we have detected an open for > tCCDebounce */
		SetStateDelayUnattached();	/* Go to the unattached state */
	else if (StateTimer == 0) /* If we have timed out (tAMETimeout) and haven't entered an alternate mode... */
		SetStateUnsupportedAccessory();	/* Go to the Unsupported.Accessory state */
}

void StateMachineUnsupportedAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	/* Grab the latest CC termination value */

	fusb_printk(K_DEBUG, "%s\n", __func__);

#ifdef SKIP_TIMER
	DebounceTimer1 = 0;
	DebounceTimer2 = 0;
	ToggleTimer = 0;
#endif

	if (CC1TermAct != CCValue) {	/* If the CC voltage has changed... */
		CC1TermAct = CCValue;	/* Store the updated value */
		DebounceTimer1 = tPDDebounceMin;	/* Reset the debounce timer to the minimum tPDDebounce */
	} else if (DebounceTimer1 == 0) {	/* If the signal has been debounced */
		DebounceTimer1 = USHRT_MAX;	/* Disable the debounce timer until we get a change */
		CC1TermDeb = CC1TermAct;	/* Store the debounced termination for CC1 */
	}
	if (CC1TermDeb == CCTypeNone)	/* If we have detected an open for > tCCDebounce */
		SetStateDelayUnattached();	/* Go to the unattached state */
}

/* /////////////////////////////////////////////////////////////////////////// */
/* State Machine Configuration */
/* /////////////////////////////////////////////////////////////////////////// */

void SetStateDisabled(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                             // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                            // Disable the 12V output */
	Registers.Power.PWR = 0x01;	/* Enter low power state */
	Registers.Control.TOGGLE = 0;	/* Disable the toggle state machine */
	Registers.Control.HOST_CUR = 0x00;	/* Disable the currents for the pull-ups (not used for UFP) */
	/* Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters */
	Registers.Switches.word = 0x0000;
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	/* Commit the control state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	/* Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here) */
	/* USBPDDisable(FALSE); */
	CC1TermDeb = CCTypeNone;	/* Clear the debounced CC1 state */
	CC2TermDeb = CCTypeNone;	/* Clear the debounced CC2 state */
	CC1TermAct = CC1TermDeb;	/* Clear the active CC1 state */
	CC2TermAct = CC2TermDeb;	/* Clear the active CC2 state */
	blnCCPinIsCC1 = FALSE;	/* Clear the CC1 pin flag */
	blnCCPinIsCC2 = FALSE;	/* Clear the CC2 pin flag */
	ConnState = Disabled;	/* Set the state machine variable to Disabled */
	StateTimer = USHRT_MAX;	/* Disable the state timer (not used in this state) */
	DebounceTimer1 = USHRT_MAX;	/* Disable the 1st level debounce timer */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer */
}

void SetStateErrorRecovery(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x01;	/* Enter low power state */
	Registers.Control.TOGGLE = 0;	/* Disable the toggle state machine */
	Registers.Control.HOST_CUR = 0x00;	/* Disable the currents for the pull-ups (not used for UFP) */
	/* Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters */
	Registers.Switches.word = 0x0000;
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	/* Commit the control state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	/* Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here) */
	/* USBPDDisable(FALSE); */
	CC1TermDeb = CCTypeNone;	/* Clear the debounced CC1 state */
	CC2TermDeb = CCTypeNone;	/* Clear the debounced CC2 state */
	CC1TermAct = CC1TermDeb;	/* Clear the active CC1 state */
	CC2TermAct = CC2TermDeb;	/* Clear the active CC2 state */
	blnCCPinIsCC1 = FALSE;	/* Clear the CC1 pin flag */
	blnCCPinIsCC2 = FALSE;	/* Clear the CC2 pin flag */
	ConnState = ErrorRecovery;	/* Set the state machine variable to ErrorRecovery */
	StateTimer = tErrorRecovery;	/* Load the tErrorRecovery duration into the state transition timer */
	DebounceTimer1 = USHRT_MAX;	/* Disable the 1st level debounce timer */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer */
}

void SetStateDelayUnattached(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* This state is only here because of the precision timing source we have with the FPGA */
	/* We are trying to avoid having the toggle state machines in sync with each other */
	/* Causing the tDRPAdvert period to overlap causing the devices to not attach for a period of time */
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
#if 0
	Registers.Power.PWR = 0x01;	/* Enter low power state */
	/* Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters */
	Registers.Switches.word = 0x0000;
	Registers.Control.TOGGLE = 0;	/* Disable the toggle state machine */
#else
	Registers.Power.PWR = 0x07;	/* Enter low power state */
	Registers.Control.WAKE_EN = 1;
	Registers.Switches.word = 0x000c;
#endif

	Registers.Control.HOST_CUR = 0x00;	/* Disable the currents for the pull-ups (not used for UFP) */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	/* Commit the control state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	/* Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here) */
	/* USBPDDisable(FALSE); */
	CC1TermDeb = CCTypeNone;	/* Clear the debounced CC1 state */
	CC2TermDeb = CCTypeNone;	/* Clear the debounced CC2 state */
	CC1TermAct = CC1TermDeb;	/* Clear the active CC1 state */
	CC2TermAct = CC2TermDeb;	/* Clear the active CC2 state */
	blnCCPinIsCC1 = FALSE;	/* Clear the CC1 pin flag */
	blnCCPinIsCC2 = FALSE;	/* Clear the CC2 pin flag */
	ConnState = DelayUnattached;	/* Set the state machine variable to delayed unattached */

	/* Set the state timer to a random value to not synchronize the toggle start
	 * (use a multiple of RAND_MAX+1 as the modulus operator)
	 */
	/*StateTimer = rand() % 64;*/
	StateTimer = 0;

	DebounceTimer1 = USHRT_MAX;	/* Disable the 1st level debounce timer */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer */
}

void SetStateUnattached(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* This function configures the Toggle state machine in the FUSB302 to handle
	 * all of the unattached states.
	 */

	/* This allows for the MCU to be placed in a low power mode until the FUSB302
	 * wakes it up upon detecting something
	 */
	/* VBUS_5V_EN = 0;                                            // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                           // Disable the 12V output */
	/* Enable the defauult host current for the pull-ups (regardless of mode) */
	Registers.Control.HOST_CUR = 0x01;
	Registers.Control.TOGGLE = 1;	/* Enable the toggle */
	if ((PortType == USBTypeC_DRP) || (blnAccSupport))	/* If we are a DRP or supporting accessories */
		Registers.Control.MODE = 1;	/* We need to enable the toggling functionality for Rp/Rd */
	else if (PortType == USBTypeC_Source)	/* If we are strictly a Source */
		Registers.Control.MODE = 3;	/* We just need to look for Rd */
	else			/* Otherwise we are a UFP */
		Registers.Control.MODE = 2;	/* So we need to only look for Rp */
	Registers.Switches.word = 0x0003;	/* Enable the pull-downs on the CC pins, toggle overrides anyway */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Measure.MDAC = MDAC_2P05V;	/* Set up DAC threshold to 2.05V */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	/* Commit the control state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	/* Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here) */
	/* USBPDDisable(FALSE); */
	ConnState = Unattached;	/* Set the state machine variable to unattached */
	SinkCurrent = utccNone;
	CC1TermDeb = CCTypeNone;	/* Clear the termination for this state */
	CC2TermDeb = CCTypeNone;	/* Clear the termination for this state */
	CC1TermAct = CC1TermDeb;
	CC2TermAct = CC2TermDeb;
	blnCCPinIsCC1 = FALSE;	/* Clear the CC1 pin flag */
	blnCCPinIsCC2 = FALSE;	/* Clear the CC2 pin flag */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	DebounceTimer1 = USHRT_MAX;	/* Disable the 1st level debounce timer, not used in this state */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer, not used in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, not used in this state */
}

void SetStateAttachWaitSnk(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0003;	/* Enable the pull-downs on the CC pins */
	if (blnCCPinIsCC1)
		Registers.Switches.MEAS_CC1 = 1;
	else
		Registers.Switches.MEAS_CC2 = 1;
	Registers.Measure.MDAC = MDAC_2P05V;	/* Set up DAC threshold to 2.05V */
	Registers.Control.HOST_CUR = 0x00;	/* Disable the host current */
	Registers.Control.TOGGLE = 0;	/* Disable the toggle */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	/* Commit the host current */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = AttachWaitSink;	/* Set the state machine variable to AttachWait.Snk */
	/* Set the current advertisment variable to none until we determine what the current is */
	SinkCurrent = utccNone;
	if (Registers.Switches.MEAS_CC1) {	/* If CC1 is what initially got us into the wait state... */
		CC1TermAct = DecodeCCTermination();	/* Determine what is initially on CC1 */
		/* Put something that we shouldn't see on the CC2 to force a debouncing */
		CC2TermAct = CCTypeNone;
	} else {
		/* Put something that we shouldn't see on the CC1 to force a debouncing */
		CC1TermAct = CCTypeNone;
		CC2TermAct = DecodeCCTermination();	/* Determine what is initially on CC2 */
	}
	CC1TermDeb = CCTypeNone;	/* Initially set to invalid */
	CC2TermDeb = CCTypeNone;	/* Initially set to invalid */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	DebounceTimer1 = tPDDebounceMax;	/* Set the tPDDebounce for validating signals to transition to */
	/* Disable the 2nd level debouncing until the first level has been debounced */
	DebounceTimer2 = USHRT_MAX;
	ToggleTimer = tFUSB302Toggle;	/* Set the toggle timer to look at each pin for tFUSB302Toggle duration */
}

void SetStateAttachWaitSrc(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0000;	/* Clear the register for the case below */
	if (blnCCPinIsCC1)	/* If we detected CC1 as an Rd */
		Registers.Switches.word = 0x0044;	/* Enable CC1 pull-up and measure */
	else
		Registers.Switches.word = 0x0088;	/* Enable CC2 pull-up and measure */
	SourceCurrent = utccDefault;	/* Set the default current level */
	UpdateSourcePowerMode();	/* Update the settings for the FUSB302 */
	Registers.Control.TOGGLE = 0;	/* Disable the toggle */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	/* Commit the toggle */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = AttachWaitSource;	/* Set the state machine variable to AttachWait.Src */
	SinkCurrent = utccNone;	/* Not used in Src */
	if (Registers.Switches.MEAS_CC1) {	/* If CC1 is what initially got us into the wait state... */
		CC1TermAct = DecodeCCTermination();	/* Determine what is initially on CC1 */
		CC2TermAct = CCTypeNone;	/* Assume that the initial value on CC2 is open */
	} else {
		CC1TermAct = CCTypeNone;	/* Assume that the initial value on CC1 is open */
		CC2TermAct = DecodeCCTermination();	/* Determine what is initially on CC2 */
	}
	CC1TermDeb = CCTypeRa;	/* Initially set both the debounced values to Ra to force the 2nd level debouncing */
	CC2TermDeb = CCTypeRa;	/* Initially set both the debounced values to Ra to force the 2nd level debouncing */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Only debounce the lines for tPDDebounce so that we can debounce a detach condition */
	DebounceTimer1 = tPDDebounceMin;
	/* Disable the 2nd level debouncing initially to force completion of a 1st level debouncing */
	DebounceTimer2 = USHRT_MAX;
	ToggleTimer = tDRP;	/* Set the initial toggle time to tDRP to ensure the other end sees the Rp */
}

void SetStateAttachWaitAcc(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0044;	/* Enable CC1 pull-up and measure */
	UpdateSourcePowerMode();
	Registers.Control.TOGGLE = 0;	/* Disable the toggle */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	/* Commit the toggle */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = AttachWaitAccessory;	/* Set the state machine variable to AttachWait.Accessory */
	SinkCurrent = utccNone;	/* Not used in accessories */
	CC1TermAct = DecodeCCTermination();	/* Determine what is initially on CC1 */
	CC2TermAct = CCTypeNone;	/* Assume that the initial value on CC2 is open */
	CC1TermDeb = CCTypeNone;	/* Initialize to open */
	CC2TermDeb = CCTypeNone;	/* Initialize to open */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states */
	DebounceTimer1 = tCCDebounceNom;
	/* Disable the 2nd level debouncing initially to force completion of a 1st level debouncing */
	DebounceTimer2 = USHRT_MAX;
	/* We're looking for the status of both lines of an accessory,
	 * no need to keep the line pull-ups on for tPDDebounce */
	ToggleTimer = tFUSB302Toggle;
}

void SetStateAttachedSrc(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 1;                                                 // Enable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	SourceCurrent = utccDefault;	/* Reset the current to the default advertisement */
	UpdateSourcePowerMode();	/* Update the source power mode */
	if (blnCCPinIsCC1 == TRUE)	/* If CC1 is detected as the CC pin... */
		Registers.Switches.word = 0x0064;	/* Configure VCONN on CC2, pull-up on CC1, measure CC1 */
	else			/* Otherwise we are assuming CC2 is CC */
		Registers.Switches.word = 0x0098;	/* Configure VCONN on CC1, pull-up on CC2, measure CC2 */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	/* Enable the USB PD state machine if applicable (no need to write to FUSB300 again), set as DFP */
	/* USBPDEnable(FALSE, TRUE); */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	/* Maintain the existing CC term values from the wait state */
	ConnState = AttachedSource;	/* Set the state machine variable to Attached.Src */
	SinkCurrent = utccNone;	/* Set the Sink current to none (not used in source) */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	DebounceTimer1 = tPDDebounceMin;	/* Set the debounce timer to tPDDebounceMin for detecting a detach */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debouncing, not needed in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, not used in this state */
}

void SetStateAttachedSink(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Control.HOST_CUR = 0x00;	/* Disable the host current */
	Registers.Measure.MDAC = MDAC_2P05V;	/* Set up DAC threshold to 2.05V */
	/* Enable the pull-downs on the CC pins, measure CC1 and disable the BMC transmitters */
	Registers.Switches.word = 0x0007;
	Registers.Switches.word = 0x0003;	/* Enable the pull-downs on the CC pins */
	if (blnCCPinIsCC1)
		Registers.Switches.MEAS_CC1 = 1;
	else
		Registers.Switches.MEAS_CC2 = 1;
	/*Enable the USB PD state machine (no need to write FUSB300 again since we are doing it here) */
	/* USBPDEnable(FALSE, FALSE); */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	/* Commit the host current */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	/* Set the state machine variable to Attached.Sink */
	ConnState = AttachedSink;
	/* Set the current advertisment variable to the default until we detect something different */
	SinkCurrent = utccDefault;
	/* Maintain the existing CC term values from the wait state */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Set the debounce timer to tPDDebounceMin for detecting changes in advertised current */
	DebounceTimer1 = tPDDebounceMin;
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer, not used in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, not used in this state */
}

void RoleSwapToAttachedSink(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Control.HOST_CUR = 0x00;	/* Disable the host current */
	Registers.Measure.MDAC = MDAC_2P05V;	/* Set up DAC threshold to 2.05V */
	if (blnCCPinIsCC1) {	/* If the CC pin is CC1... */
		Registers.Switches.PU_EN1 = 0;	/* Disable the pull-up on CC1 */
		Registers.Switches.PDWN1 = 1;	/* Enable the pull-down on CC1 */
		/* No change for CC2, it may be used as VCONN */
		CC1TermAct = CCTypeRa;	/* Initialize the CC term as open */
		CC1TermDeb = CCTypeRa;	/* Initialize the CC term as open */
	} else {
		Registers.Switches.PU_EN2 = 0;	/* Disable the pull-up on CC2 */
		Registers.Switches.PDWN2 = 1;	/* Enable the pull-down on CC2 */
		/* No change for CC1, it may be used as VCONN */
		CC2TermAct = CCTypeRa;	/* Initialize the CC term as open */
		CC2TermDeb = CCTypeRa;	/* Initialize the CC term as open */
	}
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	/* Commit the host current */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	/* Set the state machine variable to Attached.Sink */
	ConnState = AttachedSink;
	/* Set the current advertisment variable to none until we determine what the current is */
	SinkCurrent = utccNone;
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Set the debounce timer to tPDDebounceMin for detecting changes in advertised current */
	DebounceTimer1 = tPDDebounceMin;
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer, not used in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, not used in this state */
}

void RoleSwapToAttachedSource(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 1;                                                 // Enable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	UpdateSourcePowerMode();	/* Update the pull-up currents and measure block */
	if (blnCCPinIsCC1) {	/* If the CC pin is CC1... */
		Registers.Switches.PU_EN1 = 1;	/* Enable the pull-up on CC1 */
		Registers.Switches.PDWN1 = 0;	/* Disable the pull-down on CC1 */
		/* No change for CC2, it may be used as VCONN */
		CC1TermAct = CCTypeNone;	/* Initialize the CC term as open */
		CC1TermDeb = CCTypeNone;	/* Initialize the CC term as open */
	} else {
		Registers.Switches.PU_EN2 = 1;	/* Enable the pull-up on CC2 */
		Registers.Switches.PDWN2 = 0;	/* Disable the pull-down on CC2 */
		/* No change for CC1, it may be used as VCONN */
		CC2TermAct = CCTypeNone;	/* Initialize the CC term as open */
		CC2TermDeb = CCTypeNone;	/* Initialize the CC term as open */
	}
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = AttachedSource;	/* Set the state machine variable to Attached.Src */
	SinkCurrent = utccNone;	/* Set the Sink current to none (not used in Src) */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	DebounceTimer1 = tPDDebounceMin;	/* Set the debounce timer to tPDDebounceMin for detecting a detach */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debouncing, not needed in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, not used in this state */
}

void SetStateTryWaitSnk(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Switches.word = 0x0007;	/* Enable the pull-downs on the CC pins and measure on CC1 */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Measure.MDAC = MDAC_2P05V;	/* Set up DAC threshold to 2.05V */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = TryWaitSink;	/* Set the state machine variable to TryWait.Snk */
	/* Set the current advertisment variable to none until we determine what the current is */
	SinkCurrent = utccNone;
	if (Registers.Switches.MEAS_CC1) {
		CC1TermAct = DecodeCCTermination();	/* Determine what is initially on CC1 */
		CC2TermAct = CCTypeNone;	/* Assume that the initial value on CC2 is open */
	} else {
		CC1TermAct = CCTypeNone;	/* Assume that the initial value on CC1 is open */
		CC2TermAct = DecodeCCTermination();	/* Determine what is initially on CC2 */
	}
	CC1TermDeb = CCTypeNone;	/* Initially set the debounced value to none so we don't immediately detach */
	CC2TermDeb = CCTypeNone;	/* Initially set the debounced value to none so we don't immediately detach */
	StateTimer = tDRPTryWait;	/* Set the state timer to tDRPTryWait to timeout if Rp isn't detected */
	DebounceTimer1 = tPDDebounceMin;/* The 1st level debouncing is based upon tPDDebounce */
	/* Disable the 2nd level debouncing initially until we validate the 1st level */
	DebounceTimer2 = USHRT_MAX;
	/* Toggle the measure quickly (tFUSB302Toggle) to see if we detect an Rp on either */
	ToggleTimer = tFUSB302Toggle;
}

void SetStateTrySrc(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                               // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                              // Disable the 12V output */
	SourceCurrent = utccDefault;	/* Reset the current to the default advertisement */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0000;	/* Disable everything (toggle overrides anyway) */
	if (blnCCPinIsCC1) {	/* If we detected CC1 as an Rd */
		Registers.Switches.PU_EN1 = 1;	/* Enable the pull-up on CC1 */
		Registers.Switches.MEAS_CC1 = 1;	/* Measure on CC1 */
	} else {
		Registers.Switches.PU_EN2 = 1;	/* Enable the pull-up on CC1\2 */
		Registers.Switches.MEAS_CC2 = 1;	/* Measure on CC2 */
	}
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = TrySource;	/* Set the state machine variable to Try.Src */
	SinkCurrent = utccNone;	/* Not used in Try.Src */
	blnCCPinIsCC1 = FALSE;	/* Clear the CC1 is CC flag (don't know) */
	blnCCPinIsCC2 = FALSE;	/* Clear the CC2 is CC flag (don't know) */
	if (Registers.Switches.MEAS_CC1) {
		CC1TermAct = DecodeCCTermination();	/* Determine what is initially on CC1 */
		CC2TermAct = CCTypeNone;	/* Assume that the initial value on CC2 is open */
	} else {
		CC1TermAct = CCTypeNone;	/* Assume that the initial value on CC1 is open */
		CC2TermAct = DecodeCCTermination();	/* Determine what is initially on CC2 */
	}
	/* Initially set the debounced value as open until we actually debounce the signal */
	CC1TermDeb = CCTypeNone;
	/* Initially set both the active and debounce the same */
	CC2TermDeb = CCTypeNone;
	StateTimer = tDRPTry;	/* Set the state timer to tDRPTry to timeout if Rd isn't detected */
	DebounceTimer1 = tPDDebounceMin;	/* Debouncing is based soley off of tPDDebounce */
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level since it's not needed */
	/* Keep the pull-ups on for the max tPDDebounce to ensure that the other side acknowledges the pull-up */
	ToggleTimer = tPDDebounceMax;
}

void SetStateDebugAccessory(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0044;	/* Enable CC1 pull-up and measure */
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = DebugAccessory;	/* Set the state machine variable to Debug.Accessory */
	SinkCurrent = utccNone;	/* Not used in accessories */
	/* Maintain the existing CC term values from the wait state */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states */
	DebounceTimer1 = tCCDebounceNom;
	/* Disable the 2nd level debouncing initially to force completion of a 1st level debouncing */
	DebounceTimer2 = USHRT_MAX;
	/* Once we are in the debug.accessory state, we are going to stop toggling and only monitor CC1 */
	ToggleTimer = USHRT_MAX;
}

void SetStateAudioAccessory(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	Registers.Power.PWR = 0x07;	/* Enable everything except internal oscillator */
	Registers.Switches.word = 0x0044;	/* Enable CC1 pull-up and measure */
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	/* Commit the power state */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = AudioAccessory;	/* Set the state machine variable to Audio.Accessory */
	SinkCurrent = utccNone;	/* Not used in accessories */
	/* Maintain the existing CC term values from the wait state */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Once in this state, we are waiting for the lines to be
	 * stable for tCCDebounce before changing states */
	DebounceTimer1 = tCCDebounceNom;
	/* Disable the 2nd level debouncing initially to force completion of a 1st level debouncing */
	DebounceTimer2 = USHRT_MAX;
	/* Once we are in the audio.accessory state, we are going to stop toggling and only monitor CC1 */
	ToggleTimer = USHRT_MAX;
}

void SetStatePoweredAccessory(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	/* Have the option of 1.5A/3.0A for powered accessories, choosing 1.5A advert */
	SourceCurrent = utcc1p5A;
	UpdateSourcePowerMode();	/* Update the Source Power mode */
	if (blnCCPinIsCC1 == TRUE)	/* If CC1 is detected as the CC pin... */
		Registers.Switches.word = 0x0064;	/* Configure VCONN on CC2, pull-up on CC1, measure CC1 */
	else			/* Otherwise we are assuming CC2 is CC */
		Registers.Switches.word = 0x0098;	/* Configure VCONN on CC1, pull-up on CC2, measure CC2 */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	/* Maintain the existing CC term values from the wait state */
	/* TODO: The line below will be uncommented once we have full support for VDM's
	 * and can enter an alternate mode as needed for Powered.Accessories */
	/* USBPDEnable(TRUE, TRUE); */
	ConnState = PoweredAccessory;	/* Set the state machine variable to powered.accessory */
	SinkCurrent = utccNone;	/* Set the Sink current to none (not used in source) */
	/* Set the state timer to tAMETimeout (need to enter alternate mode by this time) */
	StateTimer = tAMETimeout;
	/* Set the debounce timer to the minimum tPDDebounce to check for detaches */
	DebounceTimer1 = tPDDebounceMin;
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer, not used in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, only looking at the actual CC line */
}

void SetStateUnsupportedAccessory(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* VBUS_5V_EN = 0;                                                 // Disable the 5V output... */
	/* VBUS_12V_EN = 0;                                                // Disable the 12V output */
	SourceCurrent = utccDefault;	/* Reset the current to the default advertisement for this state */
	UpdateSourcePowerMode();	/* Update the Source Power mode */
	Registers.Switches.VCONN_CC1 = 0;	/* Make sure VCONN is turned off */
	Registers.Switches.VCONN_CC2 = 0;	/* Make sure VCONN is turned off */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	/* Commit the switch state */
	udelay(250);		/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	/* Read the current state of the BC_LVL and COMP */
	ConnState = UnsupportedAccessory;	/* Set the state machine variable to unsupported.accessory */
	SinkCurrent = utccNone;	/* Set the Sink current to none (not used in source) */
	StateTimer = USHRT_MAX;	/* Disable the state timer, not used in this state */
	/* Set the debounce timer to the minimum tPDDebounce to check for detaches */
	DebounceTimer1 = tPDDebounceMin;
	DebounceTimer2 = USHRT_MAX;	/* Disable the 2nd level debounce timer, not used in this state */
	ToggleTimer = USHRT_MAX;	/* Disable the toggle timer, only looking at the actual CC line */
}

void UpdateSourcePowerMode(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	switch (SourceCurrent) {
	case utccDefault:
		/* Set up DAC threshold to 1.6V (default USB current advertisement) */
		Registers.Measure.MDAC = MDAC_1P6V;
		/* Set the host current to reflect the default USB power */
		Registers.Control.HOST_CUR = 0x01;
		break;
	case utcc1p5A:
		Registers.Measure.MDAC = MDAC_1P6V;	/* Set up DAC threshold to 1.6V */
		Registers.Control.HOST_CUR = 0x02;	/* Set the host current to reflect 1.5A */
		break;
	case utcc3p0A:
		Registers.Measure.MDAC = MDAC_2P6V;	/* Set up DAC threshold to 2.6V */
		Registers.Control.HOST_CUR = 0x03;	/* Set the host current to reflect 3.0A */
		break;
	default:		/* This assumes that there is no current being advertised */
		/* Set up DAC threshold to 1.6V (default USB current advertisement) */
		Registers.Measure.MDAC = MDAC_1P6V;
		Registers.Control.HOST_CUR = 0x00;	/* Set the host current to disabled */
		break;
	}
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	/* Commit the DAC threshold */
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	/* Commit the host current */
}

/* /////////////////////////////////////////////////////////////////////////// */
/* Type C Support Routines */
/* /////////////////////////////////////////////////////////////////////////// */

void ToggleMeasureCC1(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* If the pull-up was enabled on CC2, enable it for CC1 */
	Registers.Switches.PU_EN1 = Registers.Switches.PU_EN2;
	/* Disable the pull-up on CC2 regardless, since we aren't measuring CC2 (prevent short) */
	Registers.Switches.PU_EN2 = 0;
	Registers.Switches.MEAS_CC1 = 1;	/* Set CC1 to measure */
	Registers.Switches.MEAS_CC2 = 0;	/* Clear CC2 from measuring */
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	/* Set the switch to measure */
	/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	udelay(250);
	/* Read back the status to get the current COMP and BC_LVL */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);
}

void ToggleMeasureCC2(void)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);
	/* If the pull-up was enabled on CC1, enable it for CC2 */
	Registers.Switches.PU_EN2 = Registers.Switches.PU_EN1;
	/* Disable the pull-up on CC1 regardless, since we aren't measuring CC1 (prevent short) */
	Registers.Switches.PU_EN1 = 0;
	Registers.Switches.MEAS_CC1 = 0;	/* Clear CC1 from measuring */
	Registers.Switches.MEAS_CC2 = 1;	/* Set CC2 to measure */
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	/* Set the switch to measure */
	udelay(250);	/* Delay the reading of the COMP and BC_LVL to allow time for settling */
	/* Read back the status to get the current COMP and BC_LVL */
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);
}

CCTermType DecodeCCTermination(void)
{
	CCTermType Termination = CCTypeNone;	/* By default set it to nothing */

	fusb_printk(K_DEBUG, "%s\n", __func__);

	if (Registers.Status.COMP == 0) {	/* If COMP is high, the BC_LVL's don't matter */
		switch (Registers.Status.BC_LVL) {	/* Determine which level */
		case 0:	/* If BC_LVL is lowest... it's an vRa */
			Termination = CCTypeRa;
			break;
		case 1:	/* If BC_LVL is 1, it's default */
			Termination = CCTypeRdUSB;
			break;
		case 2:	/* If BC_LVL is 2, it's vRd1p5 */
			Termination = CCTypeRd1p5;
			break;
		default:	/* Otherwise it's vRd3p0 */
			Termination = CCTypeRd3p0;
			break;
		}
	}
	fusb_printk(K_INFO, "DecodeCCTermination Termination=%x\n", Termination);
	return Termination;	/* Return the termination type */
}

void UpdateSinkCurrent(CCTermType Termination)
{
	fusb_printk(K_DEBUG, "%s\n", __func__);

	switch (Termination) {
	case CCTypeRdUSB:	/* If we detect the default... */
	case CCTypeRa:		/* Or detect an accessory (vRa) */
		SinkCurrent = utccDefault;
		break;
	case CCTypeRd1p5:	/* If we detect 1.5A */
		SinkCurrent = utcc1p5A;
		break;
	case CCTypeRd3p0:	/* If we detect 3.0A */
		SinkCurrent = utcc3p0A;
		break;
	default:
		SinkCurrent = utccNone;
		break;
	}
}

/* /////////////////////////////////////////////////////////////////////////// */
/* Externally Accessible Routines */
/* /////////////////////////////////////////////////////////////////////////// */

void ConfigurePortType(unsigned char Control)
{
	unsigned char value;

	fusb_printk(K_DEBUG, "%s\n", __func__);

	DisableFUSB300StateMachine();
	value = Control & 0x03;
	switch (value) {
	case 1:
		PortType = USBTypeC_Source;
		fusb_printk(K_INFO, "%s USBTypeC_Source\n", __func__);

		break;
	case 2:
		PortType = USBTypeC_DRP;
		fusb_printk(K_INFO, "%s USBTypeC_DRP\n", __func__);
		break;
	default:
		PortType = USBTypeC_Sink;
		fusb_printk(K_INFO, "%s USBTypeC_Sink\n", __func__);
		break;
	}
	if (Control & 0x04)
		blnAccSupport = TRUE;
	else
		blnAccSupport = FALSE;
	if (Control & 0x08)
		blnSrcPreferred = TRUE;
	else
		blnSrcPreferred = false;

	value = (Control & 0x30) >> 4;
	switch (value) {
	case 1:
		SourceCurrent = utccDefault;
		fusb_printk(K_INFO, "%s tcc utccDefault\n", __func__);
		break;
	case 2:
		SourceCurrent = utcc1p5A;
		fusb_printk(K_INFO, "%s tcc utcc1p5A\n", __func__);
		break;
	case 3:
		SourceCurrent = utcc3p0A;
		fusb_printk(K_INFO, "%s tcc utcc3p0A\n", __func__);
		break;
	default:
		SourceCurrent = utccNone;
		fusb_printk(K_INFO, "%s tcc utccNone\n", __func__);
		break;
	}
	if (Control & 0x80)
		EnableFUSB300StateMachine();
}

void UpdateCurrentAdvert(unsigned char Current)
{
	switch (Current) {
	case 1:
		SourceCurrent = utccDefault;
		break;
	case 2:
		SourceCurrent = utcc1p5A;
		break;
	case 3:
		SourceCurrent = utcc3p0A;
		break;
	default:
		SourceCurrent = utccNone;
		break;
	}
	if (ConnState == AttachedSource)
		UpdateSourcePowerMode();
}

void GetFUSB300TypeCStatus(unsigned char abytData[])
{
	int intIndex = 0;

	abytData[intIndex++] = GetTypeCSMControl();	/* Grab a snapshot of the top level control */
	abytData[intIndex++] = ConnState & 0xFF;	/* Get the current state */
	abytData[intIndex++] = GetCCTermination();	/* Get the current CC termination */
	abytData[intIndex++] = SinkCurrent;	/* Set the sink current capability detected */
}

unsigned char GetTypeCSMControl(void)
{
	unsigned char status = 0;

	status |= (PortType & 0x03);	/* Set the type of port that we are configured as */
	switch (PortType) {	/* Set the port type that we are configured as */
	case USBTypeC_Source:
		status |= 0x01;	/* Set Source type */
		break;
	case USBTypeC_DRP:
		status |= 0x02;	/* Set DRP type */
		break;
	default:	/* If we are not DRP or Source, we are Sink which is a value of zero as initialized */
		break;
	}
	if (blnAccSupport)	/* Set the flag if we support accessories */
		status |= 0x04;
	if (blnSrcPreferred)	/* Set the flag if we prefer Source mode (as a DRP) */
		status |= 0x08;
	status |= (SourceCurrent << 4);
	if (blnSMEnabled)	/* Set the flag if the state machine is enabled */
		status |= 0x80;
	return status;
}

unsigned char GetCCTermination(void)
{
	unsigned char status = 0;

	status |= (CC1TermDeb & 0x07);	/* Set the current CC1 termination */
/* if (blnCC1Debounced)                    // Set the flag if the CC1 pin has been debounced */
/* status |= 0x08; */
	status |= ((CC2TermDeb & 0x07) << 4);	/* Set the current CC2 termination */
/* if (blnCC2Debounced)                    // Set the flag if the CC2 pin has been debounced */
/* status |= 0x80; */
	return status;
}

/* /////////////////////////////////////////////////////////////////////////// */
/* FUSB300 I2C Routines */
/* /////////////////////////////////////////////////////////////////////////// */
/* BOOL FUSB300Write(struct usbtypc *typec, unsigned char regAddr, unsigned char length, unsigned char* data) */
BOOL FUSB300Write(unsigned char regAddr, unsigned char length, unsigned char *data)
{
	int i;

	for (i = 0; i < length; i++)
		fusb300_i2c_w_reg8(typec_client, regAddr + i, data[i]);

	return true;
}

/* BOOL FUSB300Read(struct usbtypc *typec, unsigned char regAddr, unsigned char length, unsigned char* data) */
BOOL FUSB300Read(unsigned char regAddr, unsigned char length, unsigned char *data)
{
	int i;

	for (i = 0; i < length; i++)
		data[i] = fusb300_i2c_r_reg(typec_client, regAddr + i);

	return true;
}


/* /////////////////////////////////////////////////////////////////////////////// */
int register_typec_switch_callback(struct typec_switch_data *new_driver)
{
	fusb_printk(K_INFO, "Register driver %s %d\n", new_driver->name, new_driver->type);

	if (new_driver->type == DEVICE_TYPE) {
		g_exttypec->device_driver = new_driver;
		g_exttypec->device_driver->on = 0;
		return 0;
	}

	if (new_driver->type == HOST_TYPE) {
		g_exttypec->host_driver = new_driver;
		g_exttypec->host_driver->on = 0;
		if (ConnState == AttachedSource)
			trigger_driver(g_exttypec, HOST_TYPE, ENABLE, DONT_CARE);
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(register_typec_switch_callback);

int unregister_typec_switch_callback(struct typec_switch_data *new_driver)
{
	fusb_printk(K_INFO, "Unregister driver %s %d\n", new_driver->name, new_driver->type);

	if ((new_driver->type == DEVICE_TYPE) && (g_exttypec->device_driver == new_driver))
		g_exttypec->device_driver = NULL;

	if ((new_driver->type == HOST_TYPE) && (g_exttypec->host_driver == new_driver))
		g_exttypec->host_driver = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_typec_switch_callback);


static enum hrtimer_restart toggle_hrtimer_callback(struct hrtimer *timer)
{
	/* struct usbtypc *typec = container_of(timer, struct usbtypc, toggle_timer); */
	/*  */
	/* fusb_printk(K_DEBUG, "%s\n", __func__); */
	/*  */
	/* typec->ToggleTimer = 0; */
	/*  */
	/* schedule_delayed_work_on(WORK_CPU_UNBOUND, &typec->fsm_work, 0); */

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart debounce_hrtimer_callback(struct hrtimer *timer)
{
	/* struct usbtypc *typec = container_of(timer, struct usbtypc, debounce_timer); */
	/*  */
	/* fusb_printk(K_DEBUG, "%s\n", __func__); */
	/*  */
	/* typec->DebounceTimer = 0; */
	/*  */
	/* schedule_delayed_work_on(WORK_CPU_UNBOUND, &typec->fsm_work, 0); */

	return HRTIMER_NORESTART;
}

#ifdef NEVER
static int start_timer(struct hrtimer *timer, unsigned int delay, char *name)
{
	/* int ret; */
	/* ktime_t ktime; */
	/* ktime = ktime_set(0, delay * 1000000); */
	/*  */
	/* ret = hrtimer_cancel(timer); */
	/* if (ret) { */
	/* fusb_printk(K_DEBUG, "%s timer was still in use.\n", name); */
	/* //return 0; */
	/* } */
	/*  */
	/* fusb_printk(K_DEBUG,"Starting %s timer to fire in %dms\n", name, delay); */
	/*  */
	/* hrtimer_start( timer, ktime, HRTIMER_MODE_REL); */
	/*  */
	return 0;
}

static void cancel_timer(struct hrtimer *timer, char *name)
{
	/* int ret; */
	/*  */
	/* ret = hrtimer_cancel(timer); */
	/* if (ret) fusb_printk(K_DEBUG, "%s timer was still in use.\n", name); */
	/*  */
	/* fusb_printk(K_DEBUG, "%s timer module cancel.\n", name); */
}
#endif /* NEVER */

/*(on=0)=disable=(oe=1), (on=1)=enable=(oe=0)*/
static int usb3_switch_en(struct usbtypc *typec, int on)
{
	int retval = 0;

	if (!typec->pinctrl || !typec->pin_cfg || !typec->u3_sw) {
		fusb_printk(K_ERR, "%s not init\n", __func__);
		goto end;
	}

	fusb_printk(K_DEBUG, "%s on=%d\n", __func__, on);

	if (on == ENABLE) {	/*enable usb switch */
		pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_oen_low);
		typec->u3_sw->en = ENABLE;
	} else {		/*disable usb switch */
		pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_oen_high);
		typec->u3_sw->en = DISABLE;
	}

	/*fusb_printk(K_DEBUG, "%s gpio=%d\n", __func__, gpio_get_value(typec->u3_sw->en_gpio));*/
end:
	return retval;
}

/*(sel=0)=SW1=(gpio=0), (sel=1)=SW2=(gpio=1)*/
static int usb3_switch_sel(struct usbtypc *typec, int sel)
{
	int retval = 0;

	if (!typec->pinctrl || !typec->pin_cfg || !typec->u3_sw) {
		fusb_printk(K_ERR, "%s not init\n", __func__);
		goto end;
	}

	fusb_printk(K_DEBUG, "%s on=%d\n", __func__, sel);

	if (sel == UP_SIDE) {	/*select SW1 */
		pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_sel_low);
		typec->u3_sw->sel = sel;
	} else if (sel == DOWN_SIDE) {		/*select SW2 */
		pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_sel_high);
		typec->u3_sw->sel = sel;
	}

	/*fusb_printk(K_DEBUG, "%s gpio=%d\n", __func__, gpio_get_value(typec->u3_sw->sel_gpio));*/
end:
	return retval;
}

static int trigger_driver(struct usbtypc *typec, int type, int stat, int dir)
{
#ifdef CONFIG_MTK_SIB_USB_SWITCH
	if (typec->sib_enable) {
		fusb_printk(K_INFO, "SIB enable!\n");
		goto end;
	}
#endif
	fusb_printk(K_DEBUG, "trigger_driver: type:%d, stat:%d, dir%d\n", type, stat, dir);

	if (stat == ENABLE)
		usb3_switch_sel(typec, dir);

	if (type == DEVICE_TYPE && typec->device_driver) {
		if ((stat == DISABLE) && (typec->device_driver->disable)
		    && (typec->device_driver->on == ENABLE)) {
			typec->device_driver->disable(typec->device_driver->priv_data);
			typec->device_driver->on = DISABLE;

			usb_redriver_enter_dps(typec);

			usb3_switch_en(typec, DISABLE);

			fusb_printk(K_INFO, "trigger_driver: disable dev drv\n");
		} else if ((stat == ENABLE) && (typec->device_driver->enable)
			   && (typec->device_driver->on == DISABLE)) {
			typec->device_driver->enable(typec->device_driver->priv_data);
			typec->device_driver->on = ENABLE;

			usb_redriver_exit_dps(typec);

			usb3_switch_en(typec, ENABLE);

			fusb_printk(K_INFO, "trigger_driver: enable dev drv\n");
		} else {
			fusb_printk(K_INFO, "%s No device driver to enable\n", __func__);
		}
	} else if (type == HOST_TYPE && typec->host_driver) {
		if ((stat == DISABLE) && (typec->host_driver->disable)
		    && (typec->host_driver->on == ENABLE)) {
			typec->host_driver->disable(typec->host_driver->priv_data);
			typec->host_driver->on = DISABLE;

			usb_redriver_enter_dps(typec);

			usb3_switch_en(typec, DISABLE);

			fusb_printk(K_INFO, "trigger_driver: disable host drv\n");
		} else if ((stat == ENABLE) &&
			   (typec->host_driver->enable) && (typec->host_driver->on == DISABLE)) {
			typec->host_driver->enable(typec->host_driver->priv_data);
			typec->host_driver->on = ENABLE;

			usb_redriver_exit_dps(typec);

			/*ALPS02376554*/
			/*usb3_switch_en(typec, ENABLE);*/

			fusb_printk(K_INFO, "trigger_driver: enable host drv\n");
		} else {
			fusb_printk(K_INFO, "%s No device driver to enable\n", __func__);
		}
	} else {
		fusb_printk(K_INFO, "trigger_driver: no callback func\n");
	}
#ifdef CONFIG_MTK_SIB_USB_SWITCH
end:
#endif
	return 0;
}

static DEFINE_MUTEX(typec_lock);
void fusb300_eint_work(struct work_struct *data)
{
	struct usbtypc *typec = container_of(to_delayed_work(data), struct usbtypc, fsm_work);

	mutex_lock(&typec_lock);
	StateMachineFUSB300(typec);
	mutex_unlock(&typec_lock);
}

static irqreturn_t fusb300_eint_isr(int irqnum, void *data)
{
	int ret;
	struct usbtypc *typec = data;

	if (typec->en_irq) {
		fusb_printk(K_DEBUG, "Disable IRQ\n");
		disable_irq_nosync(irqnum);
		typec->en_irq = 0;
	}

	ret = schedule_delayed_work_on(WORK_CPU_UNBOUND, &typec->fsm_work, 0);

	return IRQ_HANDLED;
}

int usb3_switch_init(struct usbtypc *typec)
{
	int retval = 0;

	typec->u3_sw = kzalloc(sizeof(struct usb3_switch), GFP_KERNEL);

	typec->u3_sw->en_gpio = 251;
	typec->u3_sw->en = DISABLE;
	typec->u3_sw->sel_gpio = 252;
	typec->u3_sw->sel = DOWN_SIDE;

	/*chip enable pin*/
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_oen_init);

	/*dir selection */
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->fusb340_sel_init);

	/*fusb_printk(K_DEBUG, "en_gpio=0x%X, out=%d\n", typec->u3_sw->en_gpio,
		    gpio_get_value(typec->u3_sw->en_gpio));*/

	/*fusb_printk(K_DEBUG, "sel_gpio=0x%X, out=%d\n", typec->u3_sw->sel_gpio,
		    gpio_get_value(typec->u3_sw->sel_gpio));*/


	return retval;
}

int usb_redriver_init(struct usbtypc *typec)
{
	int retval = 0;
	int u3_eq_c1 = 197;
	int u3_eq_c2 = 196;

	typec->u_rd = kzalloc(sizeof(struct usb_redriver), GFP_KERNEL);
	typec->u_rd->c1_gpio = u3_eq_c1;
	typec->u_rd->eq_c1 = U3_EQ_HIGH;

	typec->u_rd->c2_gpio = u3_eq_c2;
	typec->u_rd->eq_c2 = U3_EQ_LOW;

	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->re_c1_init);
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->re_c2_init);

	/*fusb_printk(K_DEBUG, "c1_gpio=0x%X, out=%d\n", typec->u_rd->c1_gpio,
		    gpio_get_value(typec->u_rd->c1_gpio));*/

	/*fusb_printk(K_DEBUG, "c2_gpio=0x%X, out=%d\n", typec->u_rd->c2_gpio,
		    gpio_get_value(typec->u_rd->c2_gpio));*/

	return retval;
}

/*
 * ctrl_pin=1=C1 control pin
 * ctrl_pin=2=C2 control pin
 * (stat=0) = State=L
 * (stat=1) = State=High-Z
 * (stat=2) = State=H
 */
int usb_redriver_config(struct usbtypc *typec, int ctrl_pin, int stat)
{
	int retval = 0;
	int pin_num = 0;

	if (!typec->pinctrl || !typec->pin_cfg || !typec->u_rd) {
		fusb_printk(K_ERR, "%s not init\n", __func__);
		goto end;
	}

	fusb_printk(K_DEBUG, "%s pin=%d, stat=%d\n", __func__, ctrl_pin, stat);

	if (ctrl_pin == U3_EQ_C1) {
		pin_num = typec->u_rd->c1_gpio;
	} else if (ctrl_pin == U3_EQ_C2) {
		pin_num = typec->u_rd->c2_gpio;
	}

	switch (stat) {
	case U3_EQ_LOW:
		if (ctrl_pin == U3_EQ_C1)
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c1_low);
		else
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c2_low);
		break;
	case U3_EQ_HZ:
		if (ctrl_pin == U3_EQ_C1)
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c1_hiz);
		else
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c2_hiz);
		break;
	case U3_EQ_HIGH:
		if (ctrl_pin == U3_EQ_C1)
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c1_high);
		else
			pinctrl_select_state(typec->pinctrl,
					typec->pin_cfg->re_c2_high);
		break;
	default:
		retval = -EINVAL;
		break;
	}

	/*fusb_printk(K_DEBUG, "%s gpio=%d, out=%d\n", __func__, pin_num,
		    gpio_get_value(pin_num));*/
end:
	return retval;
}

int usb_redriver_enter_dps(struct usbtypc *typec)
{
	int retval = 0;

	retval |= usb_redriver_config(typec, U3_EQ_C1, U3_EQ_LOW);
	retval |= usb_redriver_config(typec, U3_EQ_C2, U3_EQ_LOW);
	return retval;
}

int usb_redriver_exit_dps(struct usbtypc *typec)
{
	int retval = 0;

	if (!typec->u_rd) {
		fusb_printk(K_ERR, "%s not init\n", __func__);
		goto end;
	}

	if ((typec->u_rd->eq_c1 == U3_EQ_HIGH) || (typec->u_rd->eq_c2 == U3_EQ_HIGH)) {
		retval |= usb_redriver_config(typec, U3_EQ_C1, typec->u_rd->eq_c1);
		retval |= usb_redriver_config(typec, U3_EQ_C2, typec->u_rd->eq_c2);
	} else {
		retval |= usb_redriver_config(typec, U3_EQ_C1, U3_EQ_HIGH);
		retval |= usb_redriver_config(typec, U3_EQ_C2, U3_EQ_HIGH);

		udelay(1);

		retval |= usb_redriver_config(typec, U3_EQ_C1, typec->u_rd->eq_c1);
		retval |= usb_redriver_config(typec, U3_EQ_C2, typec->u_rd->eq_c2);
	}
end:
	return retval;
}

int fusb300_eint_init(struct usbtypc *typec)
{
	int retval = 0;
	u32 ints[2] = { 0, 0 };
	struct device_node *node;
	unsigned int debounce, gpiopin;

	node = of_find_compatible_node(NULL, NULL, "mediatek,fusb300-eint");
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		debounce = ints[1];
		gpiopin = ints[0];

		gpio_set_debounce(gpiopin, debounce);
	}

	typec->irqnum = irq_of_parse_and_map(node, 0);
	typec->en_irq = 1;

	fusb_printk(K_INFO, "request_irq irqnum=0x%x\n", typec->irqnum);

	retval =
	    request_irq(typec->irqnum, fusb300_eint_isr, IRQF_TRIGGER_NONE, "fusb300_eint", typec);
	if (retval != 0) {
		fusb_printk(K_ERR, "request_irq fail, ret %d, irqnum %d!!!\n", retval,
			    typec->irqnum);
	}
	return retval;
}

void fusb300_i2c_w_reg8(struct i2c_client *client, u8 addr, u8 var)
{
	char buffer[2];

	buffer[0] = addr;
	buffer[1] = var;
	i2c_master_send(client, buffer, 2);
}

u8 fusb300_i2c_r_reg(struct i2c_client *client, u8 addr)
{
	u8 var;

	i2c_master_send(client, &addr, 1);
	i2c_master_recv(client, &var, 1);
	return var;
}

const char *strings[] = {
	"regDeviceID   ",	/* 0x01 */
	"regSwitches0  ",	/* 0x02 */
	"regSwitches1  ",	/* 0x03 */
	"regMeasure    ",	/* 0x04 */
	"regSlice      ",	/* 0x05 */
	"regControl0   ",	/* 0x06 */
	"regControl1   ",	/* 0x07 */
	"regControl2   ",	/* 0x08 */
	"regControl3   ",	/* 0x09 */
	"regMask       ",	/* 0x0A */
	"regPower      ",	/* 0x0B */
	"regReset      ",	/* 0x0C */
	"regOCPreg     ",	/* 0x0D */
	"regMaska      ",	/* 0x0E */
	"regMaskb      ",	/* 0x0F */
	"regStatus0a   ",	/* 0x3C */
	"regStatus1a   ",	/* 0x3D */
	"regInterrupta ",	/* 0x3E */
	"regInterruptb ",	/* 0x3F */
	"regStatus0    ",	/* 0x40 */
	"regStatus1    ",	/* 0x41 */
	"regInterrupt  ",	/* 0x42 */
	"regFIFO       "	/* 0x43 */
};


static int fusb300_debugfs_i2c_show(struct seq_file *s, void *unused)
{
	struct i2c_client *client = s->private;
	int i = 1;
	int str_idx = 0;
	int val = 0;

	for (; i < 0x44; i++) {
		if ((i > 0x9 && i < 0x3C))
			continue;
		val = fusb300_i2c_r_reg(client, i);
		fusb_printk(K_INFO, "%s %x\n", __func__, val);
		seq_printf(s, "[%02x]%-10s: %02x\n", i, strings[str_idx], val);
		str_idx++;
	}

	return 0;
}

static int fusb300_debugfs_i2c_open(struct inode *inode, struct file *file)
{
	return single_open(file, fusb300_debugfs_i2c_show, inode->i_private);
}

static ssize_t fusb300_debugfs_i2c_write(struct file *file,
					 const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct i2c_client *client = s->private;

	char buf[18];
	int addr = 0;
	int val = 0;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	fusb_printk(K_INFO, "%s %s\n", __func__, buf);

	if (sscanf(buf, "a0x%x v0x%x", &addr, &val) == 2) {
		val = val & 0xFF;
		addr = addr & 0xFF;
		if (addr == 0x8 || addr == 0x9 || (addr > 0x0C && addr < 0x40) || addr > 0x44) {
			fusb_printk(K_ERR, "%s invalid address=0x%x\n", __func__, addr);
			return count;
		}
		fusb_printk(K_INFO, "%s write address=0x%x, value=0x%x\n", __func__, addr, val);
		fusb300_i2c_w_reg8(client, addr, val);
		val = fusb300_i2c_r_reg(client, addr);
		fusb_printk(K_INFO, "%s result=0x%x\n", __func__, val);
	}

	return count;
}

static const struct file_operations fusb300_debugfs_i2c_fops = {
	.open = fusb300_debugfs_i2c_open,
	.write = fusb300_debugfs_i2c_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*Print U3 switch & Redriver*/
static int usb_gpio_debugfs_show(struct seq_file *s, void *unused)
{
	struct usbtypc *typec = s->private;
	int pin = 0;

	seq_puts(s, "---U3 SWITCH---\n");
	pin = typec->u3_sw->en_gpio;
	if (pin) {
		fusb_printk(K_INFO, "en=%d, out=%d\n", pin, gpio_get_value(pin));

		seq_printf(s, "OEN[%x] out=%d\n", pin, gpio_get_value(pin));
	}

	pin = typec->u3_sw->sel_gpio;
	if (pin) {
		fusb_printk(K_INFO, "sel=%d, out=%d\n", pin, gpio_get_value(pin));

		seq_printf(s, "SEL[%x] out=%d\n", pin, gpio_get_value(pin));
	}

	seq_puts(s, "---REDRIVER---\n");
	pin = typec->u_rd->c1_gpio;
	if (pin) {
		fusb_printk(K_INFO, "C1=%d, out=%d\n", pin, gpio_get_value(pin));

		seq_printf(s, "C1[%x] out=%d\n", pin, gpio_get_value(pin));
	}

	pin = typec->u_rd->c2_gpio;
	if (pin) {
		fusb_printk(K_INFO, "C2=%d, out=%d\n", pin, gpio_get_value(pin));

		seq_printf(s, "C2[%x] out=%d\n", pin, gpio_get_value(pin));
	}

	seq_puts(s, "---------\n");
	seq_puts(s, "sw [en|sel] [0|1]\n");
	seq_puts(s, "sw en 0 --> Disable\n");
	seq_puts(s, "sw en 1 --> Enable\n");
	seq_puts(s, "sw sel 0 --> sel SW1\n");
	seq_puts(s, "sw sel 1 --> sel SW2\n");
	seq_puts(s, "rd [c1|c2] [H|Z|L]\n");
	seq_puts(s, "---------\n");

	return 0;
}

static int usb_gpio_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, usb_gpio_debugfs_show, inode->i_private);
}

static ssize_t usb_gpio_debugfs_write(struct file *file,
				      const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct usbtypc *typec = s->private;

	char buf[18] = { 0 };
	char type = '\0';
	char gpio[20] = { 0 };
	int val = 0;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	fusb_printk(K_INFO, "%s %s\n", __func__, buf);

	if (sscanf(buf, "sw %s %d", gpio, &val) == 2) {
		fusb_printk(K_INFO, "%s %d\n", gpio, val);
		if (strncmp(gpio, "en", 2) == 0)
			usb3_switch_en(typec, ((val == 0) ? DISABLE : ENABLE));
		else if (strncmp(gpio, "sel", 3) == 0)
			usb3_switch_sel(typec, ((val == 0) ? UP_SIDE : DOWN_SIDE));
	} else if (sscanf(buf, "rd %s %c", gpio, &type) == 2) {
		int stat = 0;

		fusb_printk(K_INFO, "%s %c\n", gpio, val);

		if (type == 'H')
			stat = U3_EQ_HIGH;
		else if (type == 'L')
			stat = U3_EQ_LOW;
		else if (type == 'Z')
			stat = U3_EQ_HZ;

		if (strncmp(gpio, "c1", 2) == 0) {
			usb_redriver_config(typec, U3_EQ_C1, stat);
			typec->u_rd->eq_c1 = stat;
		} else if (strncmp(gpio, "c2", 2) == 0) {
			usb_redriver_config(typec, U3_EQ_C2, stat);
			typec->u_rd->eq_c2 = stat;
		}
	}

	return count;
}

static const struct file_operations usb_gpio_debugfs_fops = {
	.open = usb_gpio_debugfs_open,
	.write = usb_gpio_debugfs_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_MTK_SIB_USB_SWITCH
static int usb_sib_get(void *data, u64 *val)
{
	struct usbtypc *typec = data;

	*val = typec->sib_enable;

	fusb_printk(K_INFO, "usb_sib_get %d %llu\n", typec->sib_enable, *val);

	return 0;
}

static int usb_sib_set(void *data, u64 val)
{
	struct usbtypc *typec = data;

	typec->sib_enable = !!val;

	fusb_printk(K_INFO, "usb_sib_set %d %llu\n", typec->sib_enable, val);

	if (typec->sib_enable) {
		usb_redriver_exit_dps(typec);
		usb3_switch_en(typec, ENABLE);
	} else {
		usb_redriver_enter_dps(typec);
		usb3_switch_en(typec, DISABLE);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(usb_sib_debugfs_fops, usb_sib_get, usb_sib_set, "%llu\n");
#endif

#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT
static int usb_smt_set(void *data, u64 val)
{
	struct usbtypc *typec = data;
	int sel = val;

	fusb_printk(K_INFO, "usb_smt_set %d\n", sel);

	if (sel == 0) {
		usb_redriver_enter_dps(typec);
		usb3_switch_en(typec, DISABLE);
	} else {
		usb_redriver_exit_dps(typec);
		usb3_switch_en(typec, ENABLE);

		if (sel == 1)
			usb3_switch_sel(typec, UP_SIDE);
		else
			usb3_switch_sel(typec, DOWN_SIDE);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(usb_smt_debugfs_fops, NULL, usb_smt_set, "%llu\n");
#endif

int fusb300_init_debugfs(struct usbtypc *typec)
{
	struct dentry *root;
	struct dentry *file;
	int ret;

	root = debugfs_create_dir("usb_c", NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	file = debugfs_create_file("i2c_rw", S_IRUGO|S_IWUSR, root, typec->i2c_hd,
				   &fusb300_debugfs_i2c_fops);

	file = debugfs_create_file("gpio", S_IRUGO|S_IWUSR, root, typec, &usb_gpio_debugfs_fops);

#ifdef CONFIG_MTK_SIB_USB_SWITCH
	file = debugfs_create_file("sib", S_IRUGO|S_IWUSR, root, typec, &usb_sib_debugfs_fops);
#endif
#ifdef CONFIG_U3_PHY_SMT_LOOP_BACK_SUPPORT
	file = debugfs_create_file("smt", S_IWUSR, root, typec,
						&usb_smt_debugfs_fops);
#endif

	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
}

static int usbc_pinctrl_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct usbtypc *typec;

	if (!g_exttypec)
		g_exttypec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);

	typec = g_exttypec;

	typec->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(typec->pinctrl)) {
		fusb_printk(K_ERR, "Cannot find usb pinctrl!\n");
	} else {
		typec->pin_cfg = kzalloc(sizeof(struct usbc_pin_ctrl), GFP_KERNEL);

		fusb_printk(K_DEBUG, "pinctrl=%p\n", typec->pinctrl);

		/********************************************************/
		typec->pin_cfg->re_c1_init = pinctrl_lookup_state(typec->pinctrl, "redriver_c1_init");
		if (IS_ERR(typec->pin_cfg->re_c1_init))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c1_init\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c1_init\n");

		typec->pin_cfg->re_c1_low = pinctrl_lookup_state(typec->pinctrl, "redriver_c1_low");
		if (IS_ERR(typec->pin_cfg->re_c1_low))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c1_low\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c1_low\n");

		typec->pin_cfg->re_c1_hiz = pinctrl_lookup_state(typec->pinctrl, "redriver_c1_hiz");
		if (IS_ERR(typec->pin_cfg->re_c1_hiz))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c1_hiz\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c1_hiz\n");

		typec->pin_cfg->re_c1_high = pinctrl_lookup_state(typec->pinctrl, "redriver_c1_high");
		if (IS_ERR(typec->pin_cfg->re_c1_high))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c1_high\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c1_high\n");
		/********************************************************/

		typec->pin_cfg->re_c2_init = pinctrl_lookup_state(typec->pinctrl, "redriver_c2_init");
		if (IS_ERR(typec->pin_cfg->re_c2_init))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c2_init\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c2_init\n");

		typec->pin_cfg->re_c2_low = pinctrl_lookup_state(typec->pinctrl, "redriver_c2_low");
		if (IS_ERR(typec->pin_cfg->re_c2_low))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c2_low\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c2_low\n");

		typec->pin_cfg->re_c2_hiz = pinctrl_lookup_state(typec->pinctrl, "redriver_c2_hiz");
		if (IS_ERR(typec->pin_cfg->re_c2_hiz))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c2_hiz\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c2_hiz\n");

		typec->pin_cfg->re_c2_high = pinctrl_lookup_state(typec->pinctrl, "redriver_c2_high");
		if (IS_ERR(typec->pin_cfg->re_c2_high))
			fusb_printk(K_ERR, "Can *NOT* find redriver_c2_high\n");
		else
			fusb_printk(K_DEBUG, "Find redriver_c2_high\n");
		/********************************************************/

		typec->pin_cfg->fusb340_oen_init = pinctrl_lookup_state(typec->pinctrl, "fusb340_noe_init");
		if (IS_ERR(typec->pin_cfg->fusb340_oen_init))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_noe_init\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_noe_init\n");

		typec->pin_cfg->fusb340_oen_low = pinctrl_lookup_state(typec->pinctrl, "fusb340_noe_low");
		if (IS_ERR(typec->pin_cfg->fusb340_oen_low))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_noe_low\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_noe_low\n");

		typec->pin_cfg->fusb340_oen_high = pinctrl_lookup_state(typec->pinctrl, "fusb340_noe_high");
		if (IS_ERR(typec->pin_cfg->fusb340_oen_high))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_noe_high\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_noe_high\n");
		/********************************************************/
		typec->pin_cfg->fusb340_sel_init = pinctrl_lookup_state(typec->pinctrl, "fusb340_sel_init");
		if (IS_ERR(typec->pin_cfg->fusb340_sel_init))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_sel_init\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_sel_init\n");

		typec->pin_cfg->fusb340_sel_low = pinctrl_lookup_state(typec->pinctrl, "fusb340_sel_low");
		if (IS_ERR(typec->pin_cfg->fusb340_sel_low))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_sel_low\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_sel_low\n");

		typec->pin_cfg->fusb340_sel_high = pinctrl_lookup_state(typec->pinctrl, "fusb340_sel_high");
		if (IS_ERR(typec->pin_cfg->fusb340_sel_high))
			fusb_printk(K_ERR, "Can *NOT* find fusb340_sel_high\n");
		else
			fusb_printk(K_DEBUG, "Find fusb340_sel_high\n");
		/********************************************************/
		fusb_printk(K_INFO, "Finish parsing pinctrl\n");
	}

	return retval;
}

static int usbc_pinctrl_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id usb_pinctrl_ids[] = {
	{.compatible = "mediatek,usb_c_pinctrl",},
	{},
};

static struct platform_driver usbc_pinctrl_driver = {
	.probe = usbc_pinctrl_probe,
	.remove = usbc_pinctrl_remove,
	.driver = {
		.name = "usbc_pinctrl",
#ifdef CONFIG_OF
		.of_match_table = usb_pinctrl_ids,
#endif
	},
};

static int fusb300_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct usbtypc *typec;
	unsigned char port_type;

	fusb_printk(K_INFO, "%s 0x%x\n", __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		fusb_printk(K_ERR, "fusb300 i2c functionality check fail.\n");
		return -ENODEV;
	}

	fusb_printk(K_DEBUG, "%s %s\n", __func__, client->dev.driver->name);

	if (!g_exttypec)
		g_exttypec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);

	typec = g_exttypec;

	typec->i2c_hd = client;
	typec_client = client;

	spin_lock_init(&typec->fsm_lock);
	mutex_init(&typec_lock);

	INIT_DELAYED_WORK(&typec->fsm_work, fusb300_eint_work);

	hrtimer_init(&typec->toggle_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	typec->toggle_timer.function = toggle_hrtimer_callback;

	hrtimer_init(&typec->debounce_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	typec->debounce_timer.function = debounce_hrtimer_callback;

	fusb300_init_debugfs(typec);
	InitializeFUSB300Variables();

	/*
	   0x03 0000_0011 1:DFP 2:DRP 3:UFP
	   0x04 0000_0100 1:Acc Support, 0:Acc No Support
	   0x08 0000_1000 1:DFPPrefered
	   0x30 0011_0000 1: utccDefault 2:utcc1p5A 3:utcc3p0A other:utccNone
	   0x80 1000_0000 1:Enable SM
	 */
	port_type = 0x92;
	ConfigurePortType(port_type);

	usb_redriver_init(typec);
	usb3_switch_init(typec);
	fusb300_eint_init(typec);
	fusb_printk(K_INFO, "%s %x\n", __func__, fusb300_i2c_r_reg(client, 0x1));

	/*precheck status */
	/* StateMachineFUSB300(typec); */

	return 0;
}

#define FUSB302_NAME "FUSB302"

static const struct i2c_device_id usb_i2c_id[] = {
		{FUSB302_NAME, 0},
		{}
	};

#ifdef CONFIG_OF
static const struct of_device_id fusb302_of_match[] = {
		{.compatible = "mediatek,usb_type_c"},
		{},
	};
#endif

struct i2c_driver usb_i2c_driver = {
	.probe = fusb300_i2c_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = FUSB302_NAME,
#ifdef CONFIG_OF
		.of_match_table = fusb302_of_match,
#endif
	},
	.id_table = usb_i2c_id,
};

static int __init fusb300_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&usb_i2c_driver) != 0) {
		fusb_printk(K_ERR, "fusb300_init initialization failed!!\n");
		ret = -1;
	} else {
		fusb_printk(K_DEBUG, "fusb300_init initialization succeed!!\n");
		if (!platform_driver_register(&usbc_pinctrl_driver))
			fusb_printk(K_DEBUG, "register usbc pinctrl succeed!!\n");
		else {
			fusb_printk(K_ERR, "register usbc pinctrl fail!!\n");
			ret = -1;
		}
	}
	return ret;
}

static void __exit fusb300_exit(void)
{

}
fs_initcall(fusb300_init);
/* module_exit(fusb300_exit); */

#endif				/*USB_TYPE_C */
