/*******************************************************************************
 * @file     TypeC.h
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#ifndef _FSC_TYPEC_H_
#define	_FSC_TYPEC_H_

#include "platform.h"
#include "Port.h"

/* Type C Timing Parameters */
#define tAMETimeout     900 * TICK_SCALE_TO_MS /* Alternate Mode Entry Time */
#define tCCDebounce     120 * TICK_SCALE_TO_MS
#define tPDDebounce     15  * TICK_SCALE_TO_MS
#define tDRPTry         90  * TICK_SCALE_TO_MS
#define tDRPTryWait     600 * TICK_SCALE_TO_MS
#define tErrorRecovery  30  * TICK_SCALE_TO_MS /* Delay in Error Recov State */

#define tDeviceToggle   3   * TICK_SCALE_TO_MS /* CC pin measure toggle */
#define tTOG2           30  * TICK_SCALE_TO_MS /* DRP Toggle timing */
#define tIllegalCable   500 * TICK_SCALE_TO_MS /* Reconnect loop reset time */
#define tOrientedDebug  100 * TICK_SCALE_TO_MS /* DebugAcc Orient Delay */
#define tLoopReset      100 * TICK_SCALE_TO_MS
#define tAttachWaitPoll 20  * TICK_SCALE_TO_MS /* Periodic poll in AW.Src */
#define tAttachWaitAdv  20  * TICK_SCALE_TO_MS /* Switch from Default to correct
                                                  advertisement in AW.Src */

/* Attach-Detach loop count - Halt after N loops */
#define MAX_CABLE_LOOP  20

void StateMachineTypeC(struct Port *port);
void StateMachineDisabled(struct Port *port);
void StateMachineErrorRecovery(struct Port *port);
void StateMachineUnattached(struct Port *port);

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(struct Port *port);
void StateMachineAttachedSink(struct Port *port);
void StateMachineTryWaitSink(struct Port *port);
void StateMachineDebugAccessorySink(struct Port *port);
#endif /* FSC_HAVE_SNK */

#if (defined(FSC_HAVE_DRP) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void StateMachineTrySink(struct Port *port);
#endif /* FSC_HAVE_DRP || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(struct Port *port);
void StateMachineTryWaitSource(struct Port *port);
#ifdef FSC_HAVE_DRP
void StateMachineUnattachedSource(struct Port *port);    /* AW.Snk -> Unattached */
#endif /* FSC_HAVE_DRP */
void StateMachineAttachedSource(struct Port *port);
void StateMachineTrySource(struct Port *port);
void StateMachineDebugAccessorySource(struct Port *port);
#endif /* FSC_HAVE_SRC */

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(struct Port *port);
void StateMachineAudioAccessory(struct Port *port);
void StateMachinePoweredAccessory(struct Port *port);
void StateMachineUnsupportedAccessory(struct Port *port);
void SetStateAudioAccessory(struct Port *port);
#endif /* FSC_HAVE_ACCMODE */

void SetStateErrorRecovery(struct Port *port);
void SetStateUnattached(struct Port *port);

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(struct Port *port);
void SetStateAttachedSink(struct Port *port);
void SetStateDebugAccessorySink(struct Port *port);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(struct Port *port);
#endif /* FSC_HAVE_DRP */
void SetStateTryWaitSink(struct Port *port);
#endif /* FSC_HAVE_SNK */

#if (defined(FSC_HAVE_DRP) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void SetStateTrySink(struct Port *port);
#endif /* FSC_HAVE_DRP || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(struct Port *port);
void SetStateAttachedSource(struct Port *port);
void SetStateDebugAccessorySource(struct Port *port);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(struct Port *port);
#endif /* FSC_HAVE_DRP */
void SetStateTrySource(struct Port *port);
void SetStateTryWaitSource(struct Port *port);
#ifdef FSC_HAVE_DRP
void SetStateUnattachedSource(struct Port *port);
#endif /* FSC_HAVE_DRP */
#endif /* FSC_HAVE_SRC */

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void SetStateAttachWaitAccessory(struct Port *port);
void SetStateUnsupportedAccessory(struct Port *port);
void SetStatePoweredAccessory(struct Port *port);
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */

void SetStateIllegalCable(struct Port *port);
void StateMachineIllegalCable(struct Port *port);

void updateSourceCurrent(struct Port *port);
void updateSourceMDACHigh(struct Port *port);
void updateSourceMDACLow(struct Port *port);

void ToggleMeasure(struct Port *port);

CCTermType DecodeCCTermination(struct Port *port);
#if defined(FSC_HAVE_SRC) || \
    (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(struct Port *port);
FSC_BOOL IsCCPinRa(struct Port *port);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */
#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(struct Port *port);
#endif /* FSC_HAVE_SNK */

void UpdateSinkCurrent(struct Port *port, CCTermType term);
FSC_BOOL VbusVSafe0V (struct Port *port);

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(struct Port *port);
#endif /* FSC_HAVE_SNK */

FSC_BOOL isVSafe5V(struct Port *port);
FSC_BOOL isVBUSOverVoltage(struct Port *port, FSC_U16 vbus_mv);

void resetDebounceVariables(struct Port *port);
void setDebounceVariables(struct Port *port, CCTermType term);
void setDebounceVariables(struct Port *port, CCTermType term);
void debounceCC(struct Port *port);

#if defined(FSC_HAVE_SRC) || \
    (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void setStateSource(struct Port *port, FSC_BOOL vconn);
void DetectCCPinSource(struct Port *port);
void updateVCONNSource(struct Port *port);
void updateVCONNSource(struct Port *port);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SNK
void setStateSink(struct Port *port);
void DetectCCPinSink(struct Port *port);
void updateVCONNSource(struct Port *port);
void updateVCONNSink(struct Port *port);
#endif /* FSC_HAVE_SNK */

void clearState(struct Port *port);

void UpdateCurrentAdvert(struct Port *port, USBTypeCCurrent Current);

#ifdef FSC_DEBUG
void SetStateDisabled(struct Port *port);

/* Returns local registers as data array */
FSC_BOOL GetLocalRegisters(struct Port *port, FSC_U8 * data, FSC_S32 size);

void setAlternateModes(FSC_U8 mode);
FSC_U8 getAlternateModes(void);
#endif /* FSC_DEBUG */

#endif/* _FSC_TYPEC_H_ */
