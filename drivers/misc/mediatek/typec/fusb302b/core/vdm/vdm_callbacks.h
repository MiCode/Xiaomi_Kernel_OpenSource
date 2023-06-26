/*******************************************************************************
 * @file     vdm_callbacks.h
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
/*
 * Using this file to emulate the callbacks that a real DPM would set up for the VDM block.
 * Setting default values etc in here too.
 */


#ifndef __DPM_EMULATION_H__
#define __DPM_EMULATION_H__

#include "platform.h"
#include "vdm_types.h"
#include "PD_Types.h"

#ifdef FSC_HAVE_VDM

ModesInfo vdmRequestModesInfo(struct Port *port, FSC_U16 svid);

Identity vdmRequestIdentityInfo(struct Port *port);
SvidInfo vdmRequestSvidInfo(struct Port *port);
FSC_BOOL vdmModeEntryRequest(struct Port *port, FSC_U16 svid, FSC_U32 mode_index);
FSC_BOOL vdmModeExitRequest(struct Port *port, FSC_U16 svid, FSC_U32 mode_index);
FSC_BOOL vdmEnterModeResult(struct Port *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);

void vdmExitModeResult(struct Port *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);
void vdmInformIdentity(struct Port *port, FSC_BOOL success, SopType sop, Identity id);
void vdmInformSvids(struct Port *port, FSC_BOOL success, SopType sop, SvidInfo svid_info);
void vdmInformModes(struct Port *port, FSC_BOOL success, SopType sop, ModesInfo modes_info);
void vdmInformAttention(struct Port *port, FSC_U16 svid, FSC_U8 mode_index);
void vdmInitDpm(struct Port *port);

#endif /* FSC_HAVE_VDM */

#endif /* __DPM_EMULATION_H__ */

