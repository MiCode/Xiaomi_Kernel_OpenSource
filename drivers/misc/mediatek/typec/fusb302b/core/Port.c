/*******************************************************************************
 * @file     Port.c
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
#include "vendor_info.h"
#include "Port.h"
#include "TypeC.h"
#include "PDPolicy.h"
#include "PDProtocol.h"

#ifdef FSC_HAVE_VDM
#include "vdm_callbacks.h"
#endif /* FSC_HAVE_VDM */

/* Forward Declarations */
static void SetPortDefaultConfiguration(struct Port *port);
void InitializeRegisters(struct Port *port);
void InitializeTypeCVariables(struct Port *port);
void InitializePDProtocolVariables(struct Port *port);
void InitializePDPolicyVariables(struct Port *port);

void PortInit(struct Port *port, FSC_U8 i2cAddr)
{
    FSC_U8 i;

    port->I2cAddr = i2cAddr;
    port->PortConfig.PdRevPreferred = PD_Specification_Revision;
    port->PdRevSop = port->PortConfig.PdRevPreferred;
    port->PdRevCable = port->PortConfig.PdRevPreferred;
    SetPortDefaultConfiguration(port);
    InitializeRegisters(port);
    InitializeTypeCVariables(port);
    InitializePDProtocolVariables(port);
    InitializePDPolicyVariables(port);

    /* Add timer objects to list to make timeout checking easier */
    port->Timers[0] = &port->PDDebounceTimer;
    port->Timers[1] = &port->CCDebounceTimer;
    port->Timers[2] = &port->StateTimer;
    port->Timers[3] = &port->LoopCountTimer;
    port->Timers[4] = &port->PolicyStateTimer;
    port->Timers[5] = &port->ProtocolTimer;
    port->Timers[6] = &port->SwapSourceStartTimer;
    port->Timers[7] = &port->PpsTimer;
    port->Timers[8] = &port->VBusPollTimer;
    port->Timers[9] = &port->VdmTimer;

    for (i = 0; i < FSC_NUM_TIMERS; ++i)
    {
      TimerDisable(port->Timers[i]);
    }
}

void SetTypeCState(struct Port *port, ConnectionState state)
{
    port->ConnState = state;
    port->TypeCSubState = 0;

#ifdef FSC_DEBUG
    WriteStateLog(&port->TypeCStateLog, port->ConnState,
                  platform_get_log_time());
#endif /* FSC_DEBUG */
}

void SetPEState(struct Port *port, PolicyState_t state)
{
    port->PolicyState = state;
    port->PolicySubIndex = 0;

    port->PDTxStatus = txIdle;
    port->WaitingOnHR = FALSE;
    port->WaitInSReady = FALSE;
    notify_observers(PD_STATE_CHANGED, port->I2cAddr, 0);
#ifdef FSC_DEBUG
    WriteStateLog(&port->PDStateLog, port->PolicyState,
                  platform_get_log_time());
#endif /* FSC_DEBUG */
}

/**
 * Initalize port policy variables to default. These are changed later by
 * policy manager.
 */
static void SetPortDefaultConfiguration(struct Port *port)
{
#ifdef FSC_HAVE_SNK
    port->PortConfig.SinkRequestMaxVoltage   = 0;
    port->PortConfig.SinkRequestMaxPower     = PD_Power_as_Sink;
    port->PortConfig.SinkRequestOpPower      = PD_Power_as_Sink;
    port->PortConfig.SinkGotoMinCompatible   = FALSE;
    port->PortConfig.SinkUSBSuspendOperation = No_USB_Suspend_May_Be_Set;
    port->PortConfig.SinkUSBCommCapable      = USB_Comms_Capable;
#endif /* FSC_HAVE_SNK */

#ifdef FSC_HAVE_ACCMODE
    port->PortConfig.audioAccSupport   =Type_C_Supports_Audio_Accessory;
    port->PortConfig.poweredAccSupport =Type_C_Supports_Vconn_Powered_Accessory;
#endif /* FSC_HAVE_ACCMODE */

    port->PortConfig.RpVal = utccDefault;

    if ((Rp_Value + 1) > utccNone && (Rp_Value + 1) < utccInvalid)
        port->PortConfig.RpVal = Rp_Value + 1;

    switch (PD_Port_Type)
    {
    case 0:
        /* Consumer Only */
        port->PortConfig.PortType = USBTypeC_Sink;
        break;
    case 1:
        /* Consumer/Provider */
        if (Type_C_State_Machine == 1)
        {
            port->PortConfig.PortType = USBTypeC_Sink;
        }
        else if (Type_C_State_Machine == 2)
        {
            port->PortConfig.PortType = USBTypeC_DRP;
        }
        else
        {
            port->PortConfig.PortType = USBTypeC_UNDEFINED;
        }
        break;
    case 2:
        /* Provider/Consumer */
        if (Type_C_State_Machine == 0)
        {
            port->PortConfig.PortType = USBTypeC_Source;
        }
        else if (Type_C_State_Machine == 2)
        {
            port->PortConfig.PortType = USBTypeC_DRP;
        }
        else
        {
            port->PortConfig.PortType = USBTypeC_UNDEFINED;
        }
        break;
    case 3:
        /* Provider Only */
        port->PortConfig.PortType = USBTypeC_Source;
        break;
    case 4:
        port->PortConfig.PortType = USBTypeC_DRP;
        break;
    default:
        port->PortConfig.PortType = USBTypeC_UNDEFINED;
        break;
    }

    /* Avoid undefined port type */
    if (port->PortConfig.PortType == USBTypeC_UNDEFINED)
    {
        port->PortConfig.PortType = USBTypeC_DRP;
    }

#ifdef FSC_HAVE_DRP
    if (port->PortConfig.PortType == USBTypeC_DRP)
    {
        port->PortConfig.SrcPreferred = Type_C_Implements_Try_SRC;
        port->PortConfig.SnkPreferred = Type_C_Implements_Try_SNK;
    }
    else
    {
        port->PortConfig.SrcPreferred = FALSE;
        port->PortConfig.SnkPreferred = FALSE;
    }
#endif /* FSC_HAVE_DRP */
}

void InitializeRegisters(struct Port *port)
{
    FSC_U8 reset = 0x01;
    DeviceWrite(port->I2cAddr, regReset, 1, &reset);

    DeviceRead(port->I2cAddr, regDeviceID, 1, &port->Registers.DeviceID.byte);
    DeviceRead(port->I2cAddr, regSwitches0,1,&port->Registers.Switches.byte[0]);
    DeviceRead(port->I2cAddr, regSwitches1,1,&port->Registers.Switches.byte[1]);
    DeviceRead(port->I2cAddr, regMeasure, 1, &port->Registers.Measure.byte);
    DeviceRead(port->I2cAddr, regSlice, 1, &port->Registers.Slice.byte);
    DeviceRead(port->I2cAddr, regControl0, 1, &port->Registers.Control.byte[0]);
    DeviceRead(port->I2cAddr, regControl1, 1, &port->Registers.Control.byte[1]);
    DeviceRead(port->I2cAddr, regControl2, 1, &port->Registers.Control.byte[2]);
    DeviceRead(port->I2cAddr, regControl3, 1, &port->Registers.Control.byte[3]);
    DeviceRead(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    DeviceRead(port->I2cAddr, regPower, 1, &port->Registers.Power.byte);
    DeviceRead(port->I2cAddr, regReset, 1, &port->Registers.Reset.byte);
    DeviceRead(port->I2cAddr, regOCPreg, 1, &port->Registers.OCPreg.byte);
    DeviceRead(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    DeviceRead(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);
    DeviceRead(port->I2cAddr, regStatus0a, 1, &port->Registers.Status.byte[0]);
    DeviceRead(port->I2cAddr, regStatus1a, 1, &port->Registers.Status.byte[1]);
    DeviceRead(port->I2cAddr, regInterrupta, 1,&port->Registers.Status.byte[2]);
    DeviceRead(port->I2cAddr, regInterruptb, 1,&port->Registers.Status.byte[3]);
    DeviceRead(port->I2cAddr, regStatus0, 1, &port->Registers.Status.byte[4]);
    DeviceRead(port->I2cAddr, regStatus1, 1, &port->Registers.Status.byte[5]);
    DeviceRead(port->I2cAddr, regInterrupt, 1, &port->Registers.Status.byte[6]);
}

void InitializeTypeCVariables(struct Port *port)
{
    port->Registers.Mask.byte = 0xFF;
    DeviceWrite(port->I2cAddr, regMask, 1, &port->Registers.Mask.byte);
    port->Registers.MaskAdv.byte[0] = 0xFF;
    DeviceWrite(port->I2cAddr, regMaska, 1, &port->Registers.MaskAdv.byte[0]);
    port->Registers.MaskAdv.M_GCRCSENT = 1;
    DeviceWrite(port->I2cAddr, regMaskb, 1, &port->Registers.MaskAdv.byte[1]);

    /* Enable interrupt Pin */
    port->Registers.Control.INT_MASK = 0;
    DeviceWrite(port->I2cAddr, regControl0, 1,&port->Registers.Control.byte[0]);

    /* These two control values allow detection of Ra-Ra or Ra-Open.
     * Enabling them will allow some corner case devices to connect where
     * they might not otherwise.
     */
    port->Registers.Control.TOG_RD_ONLY = 0;
    DeviceWrite(port->I2cAddr, regControl2, 1,&port->Registers.Control.byte[2]);
    port->Registers.Control4.TOG_USRC_EXIT = 0;
    DeviceWrite(port->I2cAddr, regControl4, 1, &port->Registers.Control4.byte);

    port->TCIdle = TRUE;
    port->SMEnabled = FALSE;

    SetTypeCState(port, Disabled);

    port->DetachThreshold = VBUS_MV_VSAFE5V_DISC;
    port->CCPin = CCNone;
    port->C2ACable = FALSE;

    resetDebounceVariables(port);

#ifdef FSC_HAVE_SNK
    /* Clear the current advertisement initially */
    port->SinkCurrent = utccNone;
#endif /* FSC_HAVE_SNK */

#ifdef FSC_DEBUG
    InitializeStateLog(&port->TypeCStateLog);
#endif /* FSC_DEBUG */
}

void InitializePDProtocolVariables(struct Port *port)
{
    port->DoTxFlush = FALSE;
}

void InitializePDPolicyVariables(struct Port *port)
{
    port->isContractValid = FALSE;

    port->IsHardReset = FALSE;
    port->IsPRSwap = FALSE;

    port->WaitingOnHR = FALSE;

    port->PEIdle = TRUE;
    port->USBPDActive = FALSE;
    port->USBPDEnabled = TRUE;

#ifdef FSC_DEBUG
    port->SourceCapsUpdated = FALSE;
#endif /* FSC_DEBUG */

    /* Source Caps & Header */
    port->src_cap_header.word = 0;
    port->src_cap_header.NumDataObjects = NUMBER_OF_SRC_PDOS_ENABLED;
    port->src_cap_header.MessageType    = DMTSourceCapabilities;
    port->src_cap_header.SpecRevision   = port->PortConfig.PdRevPreferred;

    VIF_InitializeSrcCaps(port->src_caps);

    /* Sink Caps & Header */
    port->snk_cap_header.word = 0;
    port->snk_cap_header.NumDataObjects = NUMBER_OF_SNK_PDOS_ENABLED;
    port->snk_cap_header.MessageType    = DMTSinkCapabilities;
    port->snk_cap_header.SpecRevision   = port->PortConfig.PdRevPreferred;

    VIF_InitializeSnkCaps(port->snk_caps);

#ifdef FSC_HAVE_VDM
    InitializeVdmManager(port);
    vdmInitDpm(port);
    port->AutoModeEntryObjPos = -1;
    port->discoverIdCounter = 0;
    port->cblPresent = FALSE;
    port->cblRstState = CBL_RST_DISABLED;
#endif /* FSC_HAVE_VDM */

#ifdef FSC_DEBUG
    InitializeStateLog(&port->PDStateLog);
#endif /* FSC_DEBUG */
}

void SetConfiguredCurrent(struct Port *port)
{
    switch (port->PortConfig.RpVal)
    {
    case 1:
        port->SourceCurrent = utccDefault;
        break;
    case 2:
        port->SourceCurrent = utcc1p5A;
        break;
    case 3:
        port->SourceCurrent = utcc3p0A;
        break;
    default:
        port->SourceCurrent = utccNone;
        break;
    }
}
