/*******************************************************************************
 * @file     PDPolicy.h
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
#ifndef _PDPOLICY_H_
#define _PDPOLICY_H_

#include "platform.h"
#include "Port.h"

#ifdef FSC_DEBUG
#include "Log.h"
#endif /* FSC_DEBUG */

void USBPDEnable(struct Port *port, FSC_BOOL DeviceUpdate, SourceOrSink TypeCDFP);
void USBPDDisable(struct Port *port, FSC_BOOL DeviceUpdate);

void USBPDPolicyEngine(struct Port *port);
void PolicyErrorRecovery(struct Port *port);

#if (defined(FSC_HAVE_SRC) || \
     (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void PolicySourceSendHardReset(struct Port *port);
void PolicySourceSoftReset(struct Port *port, SopType sop);
void PolicySourceSendSoftReset(struct Port *port);
void PolicySourceStartup(struct Port *port);
void PolicySourceDiscovery(struct Port *port);
void PolicySourceSendCaps(struct Port *port);
void PolicySourceDisabled(struct Port *port);
void PolicySourceTransitionDefault(struct Port *port);
void PolicySourceNegotiateCap(struct Port *port);
void PolicySourceTransitionSupply(struct Port *port);
void PolicySourceCapabilityResponse(struct Port *port);
void PolicySourceReady(struct Port *port);
void PolicySourceGiveSourceCap(struct Port *port);
void PolicySourceGetSourceCap(struct Port *port);
void PolicySourceGetSinkCap(struct Port *port);
void PolicySourceSendPing(struct Port *port);
void PolicySourceGotoMin(struct Port *port);
void PolicySourceGiveSinkCap(struct Port *port);
void PolicySourceSendDRSwap(struct Port *port);
void PolicySourceEvaluateDRSwap(struct Port *port);
void PolicySourceSendVCONNSwap(struct Port *port);
void PolicySourceEvaluateVCONNSwap(struct Port *port);
void PolicySourceSendPRSwap(struct Port *port);
void PolicySourceEvaluatePRSwap(struct Port *port);
void PolicySourceWaitNewCapabilities(struct Port *port);
void PolicySourceAlertReceived(struct Port *port);
#endif /* FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE) */

#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(struct Port *port);
void PolicySinkSoftReset(struct Port *port);
void PolicySinkSendSoftReset(struct Port *port);
void PolicySinkTransitionDefault(struct Port *port);
void PolicySinkStartup(struct Port *port);
void PolicySinkDiscovery(struct Port *port);
void PolicySinkWaitCaps(struct Port *port);
void PolicySinkEvaluateCaps(struct Port *port);
void PolicySinkSelectCapability(struct Port *port);
void PolicySinkTransitionSink(struct Port *port);
void PolicySinkReady(struct Port *port);
void PolicySinkGiveSinkCap(struct Port *port);
void PolicySinkGetSinkCap(struct Port *port);
void PolicySinkGiveSourceCap(struct Port *port);
void PolicySinkGetSourceCap(struct Port *port);
void PolicySinkSendDRSwap(struct Port *port);
void PolicySinkEvaluateDRSwap(struct Port *port);
void PolicySinkSendVCONNSwap(struct Port *port);
void PolicySinkEvaluateVCONNSwap(struct Port *port);
void PolicySinkSendPRSwap(struct Port *port);
void PolicySinkEvaluatePRSwap(struct Port *port);
void PolicySinkAlertReceived(struct Port *port);
#endif /* FSC_HAVE_SNK */

void PolicyNotSupported(struct Port *port);
void PolicyInvalidState(struct Port *port);
void policyBISTReceiveMode(struct Port *port);
void policyBISTFrameReceived(struct Port *port);
void policyBISTCarrierMode2(struct Port *port);
void policyBISTTestData(struct Port *port);

#ifdef FSC_HAVE_EXT_MSG
void PolicyGetCountryCodes(struct Port *port);
void PolicyGiveCountryCodes(struct Port *port);
void PolicyGiveCountryInfo(struct Port *port);
void PolicyGetPPSStatus(struct Port *port);
void PolicyGivePPSStatus(struct Port *port);
#endif /* FSC_HAVE_EXT_MSG */

void PolicySendGenericCommand(struct Port *port);
void PolicySendGenericData(struct Port *port);

void PolicySendHardReset(struct Port *port);

FSC_U8 PolicySendCommand(struct Port *port, FSC_U8 Command, PolicyState_t nextState,
                         FSC_U8 subIndex, SopType sop);

FSC_U8 PolicySendData(struct Port *port, FSC_U8 MessageType, void* data,
                      FSC_U32 len, PolicyState_t nextState,
                      FSC_U8 subIndex, SopType sop, FSC_BOOL extMsg);

void UpdateCapabilitiesRx(struct Port *port, FSC_BOOL IsSourceCaps);

void processDMTBIST(struct Port *port);

#ifdef FSC_HAVE_VDM
/* Shim functions for VDM code */
void InitializeVdmManager(struct Port *port);
void convertAndProcessVdmMessage(struct Port *port, SopType sop);
void doVdmCommand(struct Port *port);
void doDiscoverIdentity(struct Port *port);
void doDiscoverSvids(struct Port *port);
void PolicyGiveVdm(struct Port *port);
void PolicyVdm(struct Port *port);
void autoVdmDiscovery(struct Port *port);
void PolicySendCableReset(struct Port *port);
void processCableResetState(struct Port *port);
#endif /* FSC_HAVE_VDM */

SopType TokenToSopType(FSC_U8 data);

#endif /* _PDPOLICY_H_ */
