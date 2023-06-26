/*******************************************************************************
 * @file     Port.h
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
#ifndef _PORT_H_
#define _PORT_H_

#include "TypeC_Types.h"
#include "PD_Types.h"
#include "fusb30X.h"
#include "platform.h"
#ifdef FSC_HAVE_VDM
#include "vdm_callbacks_defs.h"
#endif /* FSC_HAVE_VDM */
#ifdef FSC_HAVE_DP
#include "dp.h"
#endif /* FSC_HAVE_DP */
#include "observer.h"
#include "Log.h"
#include "dpm.h"
#include "timer.h"

/* Size of Rx/Tx FIFO protocol buffers */
#define FSC_PROTOCOL_BUFFER_SIZE 64

/* Number of timer objects in list */
#define FSC_NUM_TIMERS 10

/**
 * All state variables here for now.
 */
struct Port
{
    DevicePolicy_t*         dpm;
    PortConfig_t            PortConfig;
    FSC_U8                  PortID;                     /* Optional Port ID */
    FSC_U8                  I2cAddr;
    DeviceReg_t             Registers;
    FSC_BOOL                TCIdle;                     /* True: Type-C idle */
    FSC_BOOL                PEIdle;                     /* True: PE idle */

    /* All Type C State Machine variables */
    CCTermType              CCTerm;                     /* Active CC */
    CCTermType              CCTermCCDebounce;           /* Debounced CC */
    CCTermType              CCTermPDDebounce;
    CCTermType              CCTermPDDebouncePrevious;
    CCTermType              VCONNTerm;

    SourceOrSink            sourceOrSink;               /* TypeC Src or Snk */
    USBTypeCCurrent         SinkCurrent;                /* PP Current */
    USBTypeCCurrent         SourceCurrent;              /* Our Current */
    CCOrientation           CCPin;                      /* CC == CC1 or CC2 */
    FSC_BOOL                SMEnabled;               /* TypeC SM Enabled */
    ConnectionState         ConnState;                  /* TypeC State */
    FSC_U8                  TypeCSubState;              /* TypeC Substate */
    FSC_U16                 DetachThreshold;            /* TypeC detach level */
    FSC_U8                  loopCounter;                /* Count and prevent
                                                           attach/detach loop */
    FSC_BOOL                C2ACable;                   /* Possible C-to-A type
                                                           cable detected */
    /* All Policy Engine variables */
    PolicyState_t           PolicyState;                /* PE State */
    FSC_U8                  PolicySubIndex;             /* PE Substate */
    SopType                 PolicyMsgTxSop;             /* Tx to SOP? */
    FSC_BOOL                USBPDActive;                /* PE Active */
    FSC_BOOL                USBPDEnabled;               /* PE Enabled */
    FSC_BOOL                PolicyIsSource;             /* Policy Src/Snk? */
    FSC_BOOL                PolicyIsDFP;                /* Policy DFP/UFP? */
    FSC_BOOL                PolicyHasContract;          /* Have contract */
    FSC_BOOL                isContractValid;            /* PD Contract Valid */
    FSC_BOOL                IsHardReset;                /* HR is occurring */
    FSC_BOOL                IsPRSwap;                   /* PR is occurring */
    FSC_BOOL                IsVCONNSource;              /* VConn state */
    FSC_BOOL                USBPDTxFlag;                /* Have msg to Tx */
    FSC_U8                  CollisionCounter;           /* Collisions for PE */
    FSC_U8                  HardResetCounter;
    FSC_U8                  CapsCounter;                /* Startup caps tx'd */

    sopMainHeader_t         src_cap_header;
    sopMainHeader_t         snk_cap_header;
    doDataObject_t          src_caps[7];
    doDataObject_t          snk_caps[7];

    FSC_U8                  PdRevSop;              /* Partner spec rev */
    FSC_U8                  PdRevCable;                 /* Cable spec rev */
    FSC_BOOL                PpsEnabled;                 /* True if PPS mode */

    FSC_BOOL                WaitingOnHR;                /* HR shortcut */
    FSC_BOOL                WaitSent;                   /* Waiting on PR Swap */

    FSC_BOOL                WaitInSReady;               /* Snk/SrcRdy Delay */

    /* All Protocol State Machine Variables */
    ProtocolState_t         ProtocolState;              /* Protocol State */
    sopMainHeader_t         PolicyRxHeader;             /* Header Rx'ing */
    sopMainHeader_t         PolicyTxHeader;             /* Header Tx'ing */
    sopMainHeader_t         PDTransmitHeader;           /* Header to Tx */
    sopMainHeader_t         SrcCapsHeaderReceived;      /* Recent caps */
    sopMainHeader_t         SnkCapsHeaderReceived;      /* Recent caps */
    doDataObject_t          PolicyRxDataObj[7];         /* Rx'ing objects */
    doDataObject_t          PolicyTxDataObj[7];         /* Tx'ing objects */
    doDataObject_t          PDTransmitObjects[7];       /* Objects to Tx */
    doDataObject_t          SrcCapsReceived[7];         /* Recent caps header */
    doDataObject_t          SnkCapsReceived[7];         /* Recent caps header */
    doDataObject_t          USBPDContract;              /* Current PD request */
    doDataObject_t          SinkRequest;                /* Sink request  */
    doDataObject_t          PartnerCaps;                /* PP's Sink Caps */
    doPDO_t                 vendor_info_source[7];      /* Caps def'd by VI */
    doPDO_t                 vendor_info_sink[7];        /* Caps def'd by VI */
    SopType                 ProtocolMsgRxSop;           /* SOP of msg Rx'd */
    SopType                 ProtocolMsgTxSop;           /* SOP of msg Tx'd */
    PDTxStatus_t            PDTxStatus;                 /* Protocol Tx state */
    FSC_BOOL                ProtocolMsgRx;              /* Msg Rx'd Flag */
    FSC_BOOL                ProtocolMsgRxPending;       /* Msg in FIFO */
    FSC_U8                  ProtocolTxBytes;            /* Bytes to Tx */
    FSC_U8                  ProtocolTxBuffer[FSC_PROTOCOL_BUFFER_SIZE];
    FSC_U8                  ProtocolRxBuffer[FSC_PROTOCOL_BUFFER_SIZE];
    FSC_U8                  MessageIDCounter[SOP_TYPE_NUM]; /* Local ID count */
    FSC_U8                  MessageID[SOP_TYPE_NUM];    /* Local last msg ID */

    FSC_BOOL                DoTxFlush;                  /* Collision -> Flush */

    /* Timer objects */
    struct TimerObj         PDDebounceTimer;            /* First debounce */
    struct TimerObj         CCDebounceTimer;            /* Second debounce */
    struct TimerObj         StateTimer;                 /* TypeC state timer */
    struct TimerObj         LoopCountTimer;             /* Loop delayed clear */
    struct TimerObj         PolicyStateTimer;           /* PE state timer */
    struct TimerObj         ProtocolTimer;              /* Prtcl state timer */
    struct TimerObj         SwapSourceStartTimer;       /* PR swap delay */
    struct TimerObj         PpsTimer;                   /* PPS timeout timer */
    struct TimerObj         VBusPollTimer;              /* VBus monitor timer */
    struct TimerObj         VdmTimer;                   /* VDM timer */

    struct TimerObj         *Timers[FSC_NUM_TIMERS];

#ifdef FSC_HAVE_EXT_MSG
    ExtMsgState_t           ExtTxOrRx;                  /* Tx' or Rx'ing  */
    ExtHeader_t             ExtTxHeader;
    ExtHeader_t             ExtRxHeader;
    FSC_BOOL                ExtWaitTxRx;                /* Waiting to Tx/Rx */
    FSC_U16                 ExtChunkOffset;             /* Next chunk offset */
    FSC_U8                  ExtMsgBuffer[260];
    FSC_U8                  ExtChunkNum;                /* Next chunk number */
#else
    ExtHeader_t             ExtHeader;                  /* For sending NS */
    FSC_BOOL                WaitForNotSupported;        /* Wait for timer */
#endif

#ifdef FSC_DEBUG
    FSC_BOOL                SourceCapsUpdated;          /* GUI new caps flag */
#endif /* FSC_DEBUG */

#ifdef FSC_HAVE_VDM
    VdmDiscoveryState_t     AutoVdmState;
    VdmManager              vdmm;
    PolicyState_t           vdm_next_ps;
    PolicyState_t           originalPolicyState;
    SvidInfo                core_svid_info;
    SopType                 VdmMsgTxSop;
    doDataObject_t          vdm_msg_obj[7];
    FSC_U32                 my_mode;
    FSC_U32                 vdm_msg_length;
    FSC_BOOL                sendingVdmData;
    FSC_BOOL                VdmTimerStarted;
    FSC_BOOL                vdm_timeout;
    FSC_BOOL                vdm_expectingresponse;
    FSC_BOOL                svid_enable;
    FSC_BOOL                mode_enable;
    FSC_BOOL                mode_entered;
    FSC_BOOL                AutoModeEntryEnabled;
    FSC_S32                 AutoModeEntryObjPos;
    FSC_S16                 svid_discvry_idx;
    FSC_BOOL                svid_discvry_done;
    FSC_U16                 my_svid;
    FSC_U16                 discoverIdCounter;
    FSC_BOOL                cblPresent;
    CableResetState_t       cblRstState;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_HAVE_DP
    DisplayPortData_t       DisplayPortData;
#endif /* FSC_HAVE_DP */

#ifdef FSC_DEBUG
    StateLog                TypeCStateLog;          /* Log for TypeC states */
    StateLog                PDStateLog;             /* Log for PE states */
    FSC_U8                  USBPDBuf[PDBUFSIZE];    /* Circular PD msg buffer */
    FSC_U8                  USBPDBufStart;          /* PD msg buffer head */
    FSC_U8                  USBPDBufEnd;            /* PD msg buffer tail */
    FSC_BOOL                USBPDBufOverflow;       /* PD Log overflow flag */
#endif /* FSC_DEBUG */
};

/**
 * @brief Initializes the port structure and state machine behaviors
 *
 * Initializes the port structure with default values.
 * Also sets the i2c address of the device and enables the TypeC state machines.
 *
 * @param port Pointer to the port structure
 * @param i2c_address 8-bit value with bit zero (R/W bit) set to zero.
 * @return None
 */
void PortInit(struct Port *port, FSC_U8 i2cAddr);

/**
 * @brief Set the next Type-C state.
 *
 * Also clears substate and logs the new state value.
 *
 * @param port Pointer to the port structure
 * @param state Next Type-C state
 * @return None
 */
void SetTypeCState(struct Port *port, ConnectionState state);

/**
 * @brief Set the next Policy Engine state.
 *
 * Also clears substate and logs the new state value.
 *
 * @param port Pointer to the port structure
 * @param state Next Type-C state
 * @return None
 */
void SetPEState(struct Port *port, PolicyState_t state);

/**
 * @brief Revert the ports current setting to the configured value.
 *
 * @param port Pointer to the port structure
 * @return None
 */
void SetConfiguredCurrent(struct Port *port);

#endif /* _PORT_H_ */
