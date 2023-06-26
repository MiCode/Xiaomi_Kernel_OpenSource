/*******************************************************************************
 * @file     dpm.h
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
#ifndef MODULES_DPM_H_
#define MODULES_DPM_H_

#include "platform.h"
#include "PD_Types.h"
#include "TypeC_Types.h"
#include "vdm_types.h"

/*****************************************************************************
 * Forward declarations
 *****************************************************************************/
typedef struct devicePolicy_t DevicePolicy_t;
typedef DevicePolicy_t* DevicePolicyPtr_t;

struct Port;

/**
 * Configuration structure for port. Port looks at the structure to decide
 * policy settings. This is shared variables by both Port and DPM.
 */
typedef struct
{
    USBTypeCPort    PortType;                  /* Snk/Src/DRP */
    FSC_BOOL        SrcPreferred;              /* Source preferred (DRP) */
    FSC_BOOL        SnkPreferred;              /* Sink preferred (DRP) */
    FSC_BOOL        SinkGotoMinCompatible;     /* Sink GotoMin supported. */
    FSC_BOOL        SinkUSBSuspendOperation;   /* USB suspend capable */
    FSC_BOOL        SinkUSBCommCapable;        /* USB communications capable */
    FSC_U32         SinkRequestMaxVoltage;     /* Sink Maximum voltage */
    FSC_U32         SinkRequestMaxPower;       /* Sink Maximum power */
    FSC_U32         SinkRequestOpPower;        /* Sink Operating power */
    FSC_BOOL        audioAccSupport;           /* Audio Acc support */
    FSC_BOOL        poweredAccSupport;         /* Powered Acc support */
    FSC_BOOL        reqDRSwapToDfpAsSink;      /* Request DR swap as sink */
    FSC_BOOL        reqDRSwapToUfpAsSrc;       /* Request DR swap as source */
    FSC_BOOL        reqVconnSwapToOnAsSink;    /* Request Vconn swap */
    FSC_BOOL        reqVconnSwapToOffAsSrc;    /* Request Vconn swap */
    FSC_BOOL        reqPRSwapAsSrc;            /* Request PR swap as source */
    FSC_BOOL        reqPRSwapAsSnk;            /* Request PR swap as sink*/
    USBTypeCCurrent RpVal;                     /* Pull up value to use */
    FSC_U8          PdRevPreferred;           /* PD Rev to use */
} PortConfig_t;

/**
 * @brief Initializes the DPM object pointer
 * @param[in] DPM pointer type object
 */
void DPM_Init(DevicePolicyPtr_t *dpm);

/**
 * @brief Adds port to the list of ports managed by dpm
 * @param[in] dpm object to which the port is added
 * @param[in] port object which is added to DPM list
 */
void DPM_AddPort(DevicePolicyPtr_t dpm, struct Port *port);

/**
 * @brief Get source cap header for the port object.
 * @param[in] dpm pointer to device policy object
 * @param[in] port requesting source cap header
 */
sopMainHeader_t* DPM_GetSourceCapHeader(DevicePolicyPtr_t dpm, struct Port *port);

/**
 * @brief Get sink cap header for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting sink cap header
 */
sopMainHeader_t* DPM_GetSinkCapHeader(DevicePolicyPtr_t dpm, struct Port *port);

/**
 * @brief Get the source cap for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting source caps
 */
doDataObject_t* DPM_GetSourceCap(DevicePolicyPtr_t dpm, struct Port *port);

/**
 * @brief Get sink cap for the port object
 * @param[in] dpm pointer to device policy object
 * @param[in] port object requesting sink cap
 */
doDataObject_t* DPM_GetSinkCap(DevicePolicyPtr_t dpm, struct Port *port);

/**
 * @brief Called by the usb PD/TypeC core to ask device policy to transition
 * to the capability advertised specified by port and index.
 * @param[in] dpm pointer to the device policy object
 * @param[in] port advertising the source capability
 * @param[in] index of the source capability object
 */
FSC_BOOL DPM_TransitionSource(DevicePolicyPtr_t dpm,
                              struct Port *port, FSC_U8 index);

/**
 * @brief Called by usb PD/TypeC core to ask device policy if the source
 * is ready after the transition. It returns true if the source transition has
 * completed and successful.
 * @param[in] dpm pointer to the device policy object
 * @param[in] port advertising the source capability
 * @param[in] index of the source capability object
 */
FSC_BOOL DPM_IsSourceCapEnabled(DevicePolicyPtr_t dpm,
                                struct Port *port, FSC_U8 index);

/**
 * @brief Returns appropriate spec revision value per SOP*.
 * @param[in] current port object
 * @param[in] SOP in question
 */
SpecRev DPM_SpecRev(struct Port *port, SopType sop);

/**
 * @brief Sets appropriate spec revision value per SOP* and adjusts for
 * compatibility.
 * @param[in] current port object
 * @param[in] SOP in question
 */
void DPM_SetSpecRev(struct Port *port, SopType sop, SpecRev rev);

#ifdef FSC_HAVE_VDM
/**
 * @brief Returns appropriate SVDM revision value per SOP*.
 * @param[in] current port object
 * @param[in] SOP in question
 */
SvdmVersion DPM_SVdmVer(struct Port *port, SopType sop);
#endif /* FSC_HAVE_VDM */

/**
 * @brief Returns appropriate number of retries (based on spec rev) per SOP*.
 * @param[in] current port object
 * @param[in] SOP in question
 */
FSC_U8 DPM_Retries(struct Port *port, SopType sop);

#endif /* MODULES_DPM_H_ */
