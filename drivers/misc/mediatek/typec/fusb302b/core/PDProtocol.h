/*******************************************************************************
 * @file     PDProtocol.h
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
#ifndef _PDPROTOCOL_H_
#define	_PDPROTOCOL_H_

/////////////////////////////////////////////////////////////////////////////
//                              Required headers
/////////////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "Port.h"

#ifdef FSC_DEBUG
#include "Log.h"
#endif /* FSC_DEBUG */

/* USB PD Protocol Layer Routines */
void USBPDProtocol(struct Port *port);
void ProtocolSendCableReset(struct Port *port);
void ProtocolIdle(struct Port *port);
void ProtocolResetWait(struct Port *port);
void ProtocolRxWait(void);
void ProtocolGetRxPacket(struct Port *port, FSC_BOOL HeaderReceived);
void ProtocolTransmitMessage(struct Port *port);
void ProtocolSendingMessage(struct Port *port);
void ProtocolWaitForPHYResponse(void);
void ProtocolVerifyGoodCRC(struct Port *port);
void ProtocolSendGoodCRC(struct Port *port, SopType sop);
void ProtocolLoadSOP(struct Port *port, SopType sop);
void ProtocolLoadEOP(struct Port *port);
void ProtocolSendHardReset(struct Port *port);
void ProtocolFlushRxFIFO(struct Port *port);
void ProtocolFlushTxFIFO(struct Port *port);
void ResetProtocolLayer(struct Port *port, FSC_BOOL ResetPDLogic);

#ifdef FSC_DEBUG
/* Logging and debug functions */
FSC_BOOL StoreUSBPDToken(struct Port *port, FSC_BOOL transmitter,
                         USBPD_BufferTokens_t token);
FSC_BOOL StoreUSBPDMessage(struct Port *port, sopMainHeader_t Header,
                           doDataObject_t* DataObject,
                           FSC_BOOL transmitter, FSC_U8 SOPType);
FSC_U8 GetNextUSBPDMessageSize(struct Port *port);
FSC_U8 GetUSBPDBufferNumBytes(struct Port *port);
FSC_BOOL ClaimBufferSpace(struct Port *port, FSC_S32 intReqSize);
FSC_U8 ReadUSBPDBuffer(struct Port *port, FSC_U8* pData, FSC_U8 bytesAvail);

void SendUSBPDHardReset(struct Port *port);

#endif /* FSC_DEBUG */

#endif	/* _PDPROTOCOL_H_ */

