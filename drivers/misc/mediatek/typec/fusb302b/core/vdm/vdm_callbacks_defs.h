/*******************************************************************************
 * @file     vdm_callbacks_defs.h
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
#ifndef __FSC_VDM_CALLBACKS_DEFS_H__
#define __FSC_VDM_CALLBACKS_DEFS_H__
/*
 * This file defines types for callbacks that the VDM block will use.
 * The intention is for the user to define functions that return data
 * that VDM messages require, ex whether or not to enter a mode.
 */
#ifdef FSC_HAVE_VDM

#include "vdm_types.h"
#include "PD_Types.h"

struct Port;

typedef Identity (*RequestIdentityInfo)(struct Port *port);

typedef SvidInfo (*RequestSvidInfo)(struct Port *port);

typedef ModesInfo (*RequestModesInfo)(struct Port *port, FSC_U16);

typedef FSC_BOOL (*ModeEntryRequest)(struct Port *port, FSC_U16 svid, FSC_U32 mode_index);

typedef FSC_BOOL (*ModeExitRequest)(struct Port *port, FSC_U16 svid, FSC_U32 mode_index);

typedef FSC_BOOL (*EnterModeResult)(struct Port *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);

typedef void (*ExitModeResult)(struct Port *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);

typedef void (*InformIdentity)(struct Port *port, FSC_BOOL success, SopType sop, Identity id);

typedef void (*InformSvids)(struct Port *port, FSC_BOOL success, SopType sop, SvidInfo svid_info);

typedef void (*InformModes)(struct Port *port, FSC_BOOL success, SopType sop, ModesInfo modes_info);

typedef void (*InformAttention)(struct Port *port, FSC_U16 svid, FSC_U8 mode_index);

/*
 * VDM Manager object, so I can have multiple instances intercommunicating using the same functions!
 */
typedef struct
{
    /* callbacks! */
    RequestIdentityInfo req_id_info;
    RequestSvidInfo req_svid_info;
    RequestModesInfo req_modes_info;
    ModeEntryRequest req_mode_entry;
    ModeExitRequest req_mode_exit;
    EnterModeResult enter_mode_result;
    ExitModeResult exit_mode_result;
    InformIdentity inform_id;
    InformSvids inform_svids;
    InformModes inform_modes;
    InformAttention inform_attention;
} VdmManager;

#endif /* FSC_HAVE_VDM */

#endif /* __FSC_VDM_CALLBACKS_DEFS_H__ */

