/*******************************************************************************
 * @file     vdm_types.h
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
 * This file contains various object definitions for VDM.
 */


#ifndef __VDM_TYPES_H__
#define __VDM_TYPES_H__

#include "platform.h"

#ifdef FSC_HAVE_VDM

#define STRUCTURED_VDM_VERSION Structured_VDM_Version_SOP
#define MAX_NUM_SVIDS 12
#define MAX_MODES_PER_SVID 6

// enumeration of SVIDs
typedef enum {
    VID_UNASSIGNED = 0x0000,
    PD_SID = 0xFF00,
    DP_SID = 0xFF01
/* Define Standard/Vendor IDs Here... */
} Svid;

/*
 * All VDMs are either structured or unstructured -
 * capture that with this enumeration
 */
typedef enum {
    UNSTRUCTURED_VDM = 0x0,
    STRUCTURED_VDM = 0x1,
} VdmType;

/* Structured VDM Versions */
typedef enum {
    V1P0 = 0x0,
    V2P0 = 0x1
/* Define future Structured VDM Versions here... */
} SvdmVersion;

/* Object Position field for referring to objects in a VDM Message */
typedef enum {
    INDEX1 = 0x1,
    INDEX2 = 0x2,
    INDEX3 = 0x3,
    INDEX4 = 0x4,
    INDEX5 = 0x5,
    INDEX6 = 0x6,
    EXIT_ALL = 0x7,
} ObjPos;

/* Command Type field enumeration */
typedef enum {
    INITIATOR = 0x0,
    RESPONDER_ACK = 0x1,
    RESPONDER_NAK = 0x2,
    RESPONDER_BUSY = 0x3,
} CmdType;

/* Command field enumeration */
typedef enum {
    DISCOVER_IDENTITY = 1,
    DISCOVER_SVIDS = 2,
    DISCOVER_MODES = 3,
    ENTER_MODE = 4,
    EXIT_MODE = 5,
    ATTENTION = 6,
/* Define SVID-Specific Commands Here... */
} Command;

/* internal form factor of an Unstructured VDM Header */
typedef struct {
    Svid    svid     :16;
    VdmType vdm_type :1;
    FSC_U16 info     :15;
} UnstructuredVdmHeader;

// internal form factor of a Structured VDM Header
typedef struct {
    Svid        svid            : 16;
    VdmType     vdm_type        : 1;
    SvdmVersion svdm_version    : 2;
    ObjPos      obj_pos         : 3;
    CmdType     cmd_type        : 2;
    Command     command         : 5;
} StructuredVdmHeader;

/* Product Type field in ID Header */
typedef enum {
    UNDEFINED       =   0x0,
    HUB             =   0x1,
    PERIPHERAL      =   0x2,
    PASSIVE_CABLE   =   0x3,
    ACTIVE_CABLE    =   0x4,
    AMA             =   0x5,
} ProductType;

/* internal form factor of an ID Header */
typedef struct {
    FSC_BOOL        usb_host_data_capable   : 1;
    FSC_BOOL        usb_device_data_capable : 1;
    ProductType     product_type_ufp        : 3;
    ProductType     product_type_dfp        : 3;
    FSC_BOOL        modal_op_supported      : 1;
    FSC_U16         usb_vid                 : 16;
} IdHeader;

/* internal form factor for Cert Stat VDO */
typedef struct {
    FSC_U32 test_id : 32;
} CertStatVdo;

/* internal form factor for Product VDO */
typedef struct {
    FSC_U16 usb_product_id  : 16;
    FSC_U16     bcd_device      : 16;
} ProductVdo;

/*
 * enumeration of what I'm calling 'Cable To Letter Type'
 * ie. Type-C to Type-A/B/C
 */
typedef enum {
    TO_TYPE_A   = 0x0,
    TO_TYPE_B   = 0x1,
    TO_TYPE_C   = 0x2,
} CableToType;

/*
 * enumeration of other Type-C to [something]
 * TODO Come up with a better type name...
 */
typedef enum {
    PLUG        = 0x0,
    RECEPTACLE  = 0x1,
} CableToPr;

/*
 * enumeration of cable latency values
 * in nanoseconds (max) eg NS20 means 10ns-20ns latency.
 */
typedef enum {
    NS10            = 0x1,
    NS20            = 0x2,
    NS30            = 0x3,
    NS40            = 0x4,
    NS50            = 0x5,
    NS60            = 0x6,
    NS70            = 0x7,
    NS1000          = 0x8,
    NS2000          = 0x9,
    NS3000          = 0xa,
} CableLatency;

/* enumeration of Cable Termination Type */
typedef enum {
    /* note: NO_VCONN means VCONN not _required_ */
    BOTH_PASSIVE_NO_VCONN   = 0x0,
    /* VCONN required */
    BOTH_PASSIVE_VCONN      = 0x1,
    ONE_EACH_VCONN          = 0x2,
    BOTH_ACTIVE_VCONN       = 0x3,
} CableTermType;

/* enumeration for configurability of SS Directionality */
typedef enum {
    FIXED           = 0x0,
    CONFIGURABLE    = 0x1,
} SsDirectionality;

/* enumeration for VBUS Current Handling Capability */
typedef enum {
    VBUS_1P5A   = 0x0, // 1.5 Amps
    VBUS_3A     = 0x1,
    VBUS_5A     = 0x2,
} VbusCurrentHandlingCapability;

/* enumeration for VBUS through cable-ness */
typedef enum {
    NO_VBUS_THRU_CABLE  = 0x0,
    VBUS_THRU_CABLE     = 0x1,
} VbusThruCable;

/* enumeration for SOP'' presence */
typedef enum {
    SOP2_NOT_PRESENT    = 0x0,
    SOP2_PRESENT        = 0x1,
} Sop2Presence;

/* enumeration for USB Superspeed Signaling Support */
typedef enum {
    USB2P0_ONLY     = 0x0,
    USB3P1_GEN1     = 0x1,
    /* Gen 1 and Gen 2 */
    USB3P1_GEN1N2   = 0x2,
} UsbSsSupport;

/* internal form factor for Cable VDO */
typedef struct {
    FSC_U8                          cable_hw_version            : 4;
    FSC_U8                          cable_fw_version            : 4;
    CableToType                     cable_to_type               : 2;
    CableToPr                       cable_to_pr                 : 1;
    CableLatency                    cable_latency               : 4;
    CableTermType                   cable_term                  : 2;
    SsDirectionality                sstx1_dir_supp              : 1;
    SsDirectionality                sstx2_dir_supp              : 1;
    SsDirectionality                ssrx1_dir_supp              : 1;
    SsDirectionality                ssrx2_dir_supp              : 1;
    VbusCurrentHandlingCapability   vbus_current_handling_cap   : 2;
    VbusThruCable                   vbus_thru_cable             : 1;
    Sop2Presence                    sop2_presence               : 1;
    UsbSsSupport                    usb_ss_supp                 : 3;
} CableVdo;

/* enumeration for power needed by adapter for full functionality */
typedef enum {
    VCONN_1W    = 0x0,
    VCONN_1P5W  = 0x1,
    VCONN_2W    = 0x2,
    VCONN_3W    = 0x3,
    VCONN_4W    = 0x4,
    VCONN_5W    = 0x5,
    VCONN_6W    = 0x6,
} VConnFullPower;

/* enumeration for VCONN being required by an adapter */
typedef enum {
    VCONN_NOT_REQUIRED =    0x0,
    VCONN_REQUIRED =        0x1,
} VConnRequirement;

/* enumeration for VBUS being required by an adapter */
typedef enum {
    VBUS_NOT_REQUIRED =     0x0,
    VBUS_REQUIRED =         0x1,
} VBusRequirement;

/*
 * enumeration for USB Superspeed Signaling support by an AMA...
 * very similar to the USB SS Support for Cable VDO, but a little
 * different in the PD spec.
 */
typedef enum {
    AMA_USB2P0_ONLY         = 0x0,
    AMA_GEN1                = 0x1,
    AMA_GEN1N2              = 0x2,
    AMA_USB2P0_BILLBOARD    = 0x3,
} AmaUsbSsSupport;

/* internal form factor for Alternate Mode Adapter VDO */
typedef struct {
    AmaUsbSsSupport                 usb_ss_supp         : 3;
    VBusRequirement                 vbus_requirement    : 1;
    VConnRequirement                vconn_requirement   : 1;
    VConnFullPower                  vconn_full_power    : 3;
    FSC_U16                         reserved            : 13;
    FSC_U8                          vdo_version         : 3;
    FSC_U8                          cable_fw_version    : 4;
    FSC_U8                          cable_hw_version    : 4;
} AmaVdo;

/* internal form factor for an SVID VDO */
typedef struct {
    FSC_U16 SVID0   : 16;
    FSC_U16 SVID1   : 16;
} SvidVdo;

/*
 * 'Identity' Object - this is how we'll store data received from
 * Discover Identity messages
 */
typedef struct {
    /* set to true to nack a Discover Identity */
    FSC_BOOL    nack;
    /* TODO: also put BUSY answer in here */
    IdHeader    id_header;
    CertStatVdo cert_stat_vdo;
    FSC_BOOL    has_product_vdo;
    ProductVdo  product_vdo;
    FSC_BOOL    has_cable_vdo;
    CableVdo    cable_vdo;
    FSC_BOOL    has_ama_vdo;
    AmaVdo      ama_vdo;
} Identity;

/*
 * SVID Info object - give the system an easy struct to pass info to us
 * through a callback
 */
typedef struct {
    /* set to true to NACK the Discover SVIDs */
    FSC_BOOL    nack;
    /* TODO: incorporate BUSY */
    FSC_U32     num_svids;
    FSC_U16     svids[MAX_NUM_SVIDS];
} SvidInfo;

/*
 * Modes Info object  give the system an easy
 * struct to pass info to us through a callback
 */
typedef struct {
    /* set to true to NACK the Discover SVIDs */
    FSC_BOOL    nack;
    /* TODO: incorporate BUSY */
    FSC_U16     svid;
    FSC_U32     num_modes;
    FSC_U32     modes[MAX_MODES_PER_SVID];
} ModesInfo;

#endif /* FSC_HAVE_VDM */

#endif /* __VDM_TYPES_H__ */
