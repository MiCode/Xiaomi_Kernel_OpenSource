/*******************************************************************************
 * @file     dp.c
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
#include "dp.h"
#include "vdm.h"
#include "bitfield_translators.h"
#include "PD_Types.h"

#ifdef FSC_HAVE_DP

void DP_Initialize(struct Port *port)
{
    DisplayPortCaps_t *cap = &port->DisplayPortData.DpCap;
    /* Initialize display port capabilities */
    cap->word = 0;
    cap->UfpDCapable = DisplayPort_UFP_Capable;
    cap->DfpDCapable = DisplayPort_DFP_Capable;
    cap->Transport = DisplayPort_Signaling;
    cap->ReceptacleIndication = DisplayPort_Receptacle;
    cap->USB2p0NotUsed = !DisplayPort_USBr2p0Signal_Req;
    cap->DFP_DPinAssignments = DisplayPort_DFP_Pin_Mask;
    cap->UFP_DPinAssignments = DisplayPort_UFP_Pin_Mask;
    /* Initialize other variables */
    port->DisplayPortData.DpConfigured = FALSE;
    port->DisplayPortData.DpStatus.word = 0;
    port->DisplayPortData.DpCapMatched = FALSE;
    port->DisplayPortData.DpPpConfig.word = 0;
    port->DisplayPortData.DpPpStatus.word = 0;
    port->DisplayPortData.DpModeEntered = 0;
    port->DisplayPortData.DpAutoModeEntryEnabled = DisplayPort_Auto_Mode_Entry;
    port->DisplayPortData.DpEnabled = DisplayPort_Enabled;

    if (cap->UfpDCapable) {
        port->DisplayPortData.DpStatus.Connection |= DP_MODE_UFP_D;
    }
    if (cap->DfpDCapable) {
        port->DisplayPortData.DpStatus.Connection |= DP_MODE_DFP_D;
    }
}

void DP_RequestPartnerStatus(struct Port *port)
{
    doDataObject_t svdmh = { 0 };
    FSC_U32 length = 0;
    FSC_U32 arr[2] = { 0 };
    svdmh.SVDM.SVID = DP_SID;
    svdmh.SVDM.VDMType = STRUCTURED_VDM;
    svdmh.SVDM.Version = DPM_SVdmVer(port, SOP_TYPE_SOP);
    /* saved mode position */
    svdmh.SVDM.ObjPos = port->DisplayPortData.DpModeEntered & 0x7;
    svdmh.SVDM.CommandType = INITIATOR;
    svdmh.SVDM.Command = DP_COMMAND_STATUS;
    arr[0] = svdmh.object;
    length++;
    arr[1] = port->DisplayPortData.DpStatus.word;
    length++;
    port->originalPolicyState = port->PolicyState;
    sendVdmMessageWithTimeout(port, SOP_TYPE_SOP, arr, length,
                              peDpRequestStatus);
}

void DP_RequestPartnerConfig(struct Port *port, DisplayPortConfig_t in)
{
    doDataObject_t svdmh = { 0 };
    FSC_U32 length = 0;
    FSC_U32 arr[2] = { 0 };
    svdmh.SVDM.SVID = DP_SID;
    svdmh.SVDM.VDMType = STRUCTURED_VDM;
    svdmh.SVDM.Version = DPM_SVdmVer(port, SOP_TYPE_SOP);
    svdmh.SVDM.ObjPos = port->DisplayPortData.DpModeEntered & 0x7;
    svdmh.SVDM.CommandType = INITIATOR;
    svdmh.SVDM.Command = DP_COMMAND_CONFIG;
    arr[0] = svdmh.object;
    length++;
    arr[1] = in.word;
    length++;
    port->originalPolicyState = port->PolicyState;
    sendVdmMessageWithTimeout(port, SOP_TYPE_SOP, arr, length,
                              peDpRequestConfig);
}


void DP_SetPortMode(struct Port *port, DisplayPortMode_t conn)
{
    switch (conn)
    {
    case DP_MODE_DISABLED:
      port->DisplayPortData.DpCap.UfpDCapable = 0;
      port->DisplayPortData.DpCap.DfpDCapable = 0;
      port->DisplayPortData.DpCap.UFP_DPinAssignments = 0;
      port->DisplayPortData.DpCap.DFP_DPinAssignments = 0;
      port->DisplayPortData.DpStatus.Connection = DP_MODE_DISABLED;
      break;
    case DP_MODE_UFP_D:
      port->DisplayPortData.DpCap.UfpDCapable = DisplayPort_UFP_Capable;
      port->DisplayPortData.DpCap.DfpDCapable = 0;
      port->DisplayPortData.DpCap.UFP_DPinAssignments = DisplayPort_UFP_Pin_Mask;
      port->DisplayPortData.DpCap.DFP_DPinAssignments = 0;
      port->DisplayPortData.DpStatus.Connection = DP_MODE_UFP_D;
      break;
    case DP_MODE_DFP_D:
      port->DisplayPortData.DpCap.UfpDCapable = 0;
      port->DisplayPortData.DpCap.DfpDCapable = DisplayPort_DFP_Capable;
      port->DisplayPortData.DpCap.UFP_DPinAssignments = 0;
      port->DisplayPortData.DpCap.DFP_DPinAssignments = DisplayPort_DFP_Pin_Mask;
      port->DisplayPortData.DpStatus.Connection = DP_MODE_DFP_D;
      break;
    case DP_MODE_BOTH:
      port->DisplayPortData.DpCap.UfpDCapable = DisplayPort_UFP_Capable;
      port->DisplayPortData.DpCap.DfpDCapable = DisplayPort_DFP_Capable;
      port->DisplayPortData.DpCap.UFP_DPinAssignments = DisplayPort_UFP_Pin_Mask;
      port->DisplayPortData.DpCap.DFP_DPinAssignments = DisplayPort_DFP_Pin_Mask;
      port->DisplayPortData.DpStatus.Connection = DP_MODE_BOTH;
      break;
    default:
      break;
    }
}

static FSC_BOOL DP_SelectPinAssignment(struct Port *port)
{
    DisplayPortConfig_t *config;        /* sent to port partner Type-C UFP_U */
    DisplayPortCaps_t   *dpPartnerCap;    /* type-C UFP capability */
    FSC_U32 dp_pin_match;

    dpPartnerCap = &port->DisplayPortData.DpPpCap;
    config = &port->DisplayPortData.DpPpConfig;

    /* Plug indicates connections receptacle must use */
    config->PinAssign = DP_PIN_ASSIGN_NS;
    if (config->Mode == DP_CONF_UFP_D)
    {
       /* Configuring as display port sink */
       if (dpPartnerCap->ReceptacleIndication == DP_Plug)
       {
         /* Plug describes receptacle's pin assignment */
         dp_pin_match = dpPartnerCap->DFP_DPinAssignments &
                        port->DisplayPortData.DpCap.DFP_DPinAssignments;
       }
       else
       {
         dp_pin_match = dpPartnerCap->UFP_DPinAssignments &
                        port->DisplayPortData.DpCap.DFP_DPinAssignments;
       }
     }
     else
     {
       if (dpPartnerCap->ReceptacleIndication == DP_Plug)
       {
         dp_pin_match = dpPartnerCap->UFP_DPinAssignments &
                        port->DisplayPortData.DpCap.UFP_DPinAssignments;
       }
       else
       {
         dp_pin_match = dpPartnerCap->DFP_DPinAssignments &
                        port->DisplayPortData.DpCap.UFP_DPinAssignments;
       }
     }

     /* Match pins assignment C, D, E in order of preference. Lets start with
      * lowest preference available and then move up */
     config->PinAssign = ((DisplayPort_UFP_PinAssign_Start <= 'E') &&
                          (dp_pin_match & DP_PIN_ASSIGN_E)) ?
                           DP_PIN_ASSIGN_E : DP_PIN_ASSIGN_NS;
     config->PinAssign = ((DisplayPort_UFP_PinAssign_Start <= 'D') &&
                          (dp_pin_match & DP_PIN_ASSIGN_D)) ?
                           DP_PIN_ASSIGN_D : config->PinAssign;
     config->PinAssign = ((DisplayPort_UFP_PinAssign_Start <= 'C') &&
                          (dp_pin_match & DP_PIN_ASSIGN_C)) ?
                           DP_PIN_ASSIGN_C : config->PinAssign;

     if (config->PinAssign == DP_PIN_ASSIGN_NS)
     {
       return FALSE;
     }
     return TRUE;
}

static FSC_BOOL DP_SelectRole(struct Port *port)
{
    DisplayPortConfig_t *config;        /* sent to port partner Type-C UFP_U */
    DisplayPortCaps_t *dpPartnerCap;    /* type-C UFP capability */

    dpPartnerCap = &port->DisplayPortData.DpPpCap;
    config = &port->DisplayPortData.DpPpConfig;

    /* Select whether to configure partner (UFP_U) as DPF_D or UFP_D */
    if (dpPartnerCap->UfpDCapable &&
        port->DisplayPortData.DpCap.DfpDCapable)
    {
      if (DisplayPort_Preferred_Snk &&
          dpPartnerCap->DfpDCapable &&
          port->DisplayPortData.DpCap.UfpDCapable)
      {
        /* DFP_D capable and preferred */
        config->Mode = DP_CONF_DFP_D;
      }
      else
      {
        config->Mode = DP_CONF_UFP_D;
      }
      return TRUE;
    }
    else if (dpPartnerCap->DfpDCapable &&
             port->DisplayPortData.DpCap.UfpDCapable)
    {
      config->Mode = DP_CONF_DFP_D;
      return TRUE;
    }

    return FALSE;
}

/**
 * @brief This function Evaluates the UFP_U capability and selects UFP_U
 * configuration that will be used. The configuration is set in configuration
 * VDM.
 */
FSC_BOOL DP_EvaluateSinkCapability(struct Port *port, FSC_U32 mode_in)
{
    DisplayPortConfig_t *config = NULL;        /* sent to port partner Type-C UFP_U */
    DisplayPortCaps_t *dpPartnerCap = NULL;    /* type-C UFP capability */

    port->DisplayPortData.DpCapMatched = FALSE;

    /* If display port features are disabled return */
    if (port->DisplayPortData.DpEnabled == FALSE) return FALSE;
    if (port->DisplayPortData.DpAutoModeEntryEnabled == FALSE) return FALSE;

    dpPartnerCap = &port->DisplayPortData.DpPpCap;
    config = &port->DisplayPortData.DpPpConfig;

    /* Copy the capability from the mode ACK*/
    dpPartnerCap->word = mode_in;
    config->word = 0;

    if (FALSE == DP_SelectRole(port))
    {
      return FALSE;
    }

    /* Match USB 2.0 signaling */
    if (dpPartnerCap->USB2p0NotUsed == 0 && DisplayPort_USBr2p0Signal_Req)
    {
      return FALSE;
    }
    config->SignalConfig = DisplayPort_Signaling;

    if (DP_SelectPinAssignment(port) != TRUE)
    {
      return FALSE;
    }

    port->DisplayPortData.DpCapMatched = TRUE;
    return TRUE;
}

FSC_BOOL DP_ProcessCommand(struct Port *port, FSC_U32* arr_in)
{
    doDataObject_t svdmh_in = {0};
    DisplayPortStatus_t stat;
    DisplayPortConfig_t config;
    FSC_BOOL is_ack = FALSE;

    if (port->DisplayPortData.DpEnabled == FALSE) return TRUE;
    svdmh_in.object = arr_in[0];
    is_ack = (svdmh_in.SVDM.CommandType == RESPONDER_ACK) ? TRUE : FALSE;

    switch (svdmh_in.SVDM.Command)
    {
    case DP_COMMAND_ATTENTION:
      if (svdmh_in.SVDM.CommandType == INITIATOR)
      {
        stat.word = arr_in[1];
        /* Attention can occur anytime after enter mode */
        if (port->AutoVdmState > AUTO_VDM_ENTER_MODE_PP &&
            port->AutoVdmState < AUTO_VDM_DONE)
        {
          DP_UpdatePartnerStatus(port, stat, TRUE);
          /* let vdm state machine continue if already active */
          break;
        }
        else if (port->PolicyState == peDpRequestStatus ||
                 port->PolicyState == peDpRequestConfig)
        {
          /* Interrupted by attention during request for stat/config.
           * Restore the original policy and continue.
           */
          SetPEState(port, port->originalPolicyState);
        }
        DP_ProcessPartnerAttention(port, stat);
        DP_UpdatePartnerStatus(port, stat, TRUE);
      }
      else
      {
        /* Restore policy state after getting response */
        SetPEState(port, port->originalPolicyState);
      }
      break;
    case DP_COMMAND_STATUS:
      if (svdmh_in.SVDM.CommandType == INITIATOR)
      {
        stat.word = arr_in[1];
        /* Copy the partner status */
        DP_UpdatePartnerStatus(port, stat, TRUE);
        /* Send the port status */
        DP_SendPortStatus(port, svdmh_in);
      }
      else
      {
        stat.word = arr_in[1];
        /* Copy port partner status update from response */
        DP_UpdatePartnerStatus(port, stat, is_ack);
        /* Logging only: state ack/nak */
        SetPEState(port, (is_ack == TRUE) ? peDpRequestStatusAck :
                                            peDpRequestStatusNak);
        /* Restore policy state after getting response */
        SetPEState(port, port->originalPolicyState);
      }
      break;
    case DP_COMMAND_CONFIG:
      if (svdmh_in.SVDM.CommandType == INITIATOR)
      {
        config.word = arr_in[1];
        if (DP_ProcessConfigRequest(port, config) == TRUE)
        {
          /* if pin reconfig is successful */
          DP_SendPortConfig(port, svdmh_in, TRUE);
          port->DisplayPortData.DpConfigured = TRUE;
        }
      }
      else
      {
        /* Inform system of success/failure response */
        DP_ProcessConfigResponse(port, is_ack);
        /* Logging only: state ack/nak */
        SetPEState(port, (is_ack == TRUE) ? peDpRequestConfigAck :
                                            peDpRequestConfigNak);
        /* Restore policy state after getting response */
        SetPEState(port, port->originalPolicyState);
      }
      break;
    default:
      /* command not recognized */
      return TRUE;
    }
    return FALSE;
}

void DP_SendPortStatus(struct Port *port, doDataObject_t svdmh_in)
{
    doDataObject_t svdmh_out = {0};
    FSC_U32 length_out = 0;
    FSC_U32 arr_out[2] = {0};

    /*  Reflect most fields */
    svdmh_out.object = svdmh_in.object;
    svdmh_out.SVDM.Version = DPM_SVdmVer(port, SOP_TYPE_SOP);
    svdmh_out.SVDM.CommandType = RESPONDER_ACK;
    arr_out[0] = svdmh_out.object;
    length_out++;
    arr_out[1] = port->DisplayPortData.DpStatus.word;
    length_out++;

    sendVdmMessage(port, SOP_TYPE_SOP, arr_out, length_out,
                   port->PolicyState);
}

void DP_SendPortConfig(struct Port *port, doDataObject_t svdmh_in,
                                FSC_BOOL success)
{
    doDataObject_t svdmh_out = {0};
    FSC_U32 length_out = 0;
    FSC_U32 arr_out[2] = {0};

    /* a callback to platform to put all dp pins to safe state */
    platform_dp_enable_pins(FALSE, 0);

    /*  Reflect most fields */
    svdmh_out.object = svdmh_in.object;
    svdmh_out.SVDM.Version = DPM_SVdmVer(port, SOP_TYPE_SOP);
    svdmh_out.SVDM.CommandType = success == TRUE ? RESPONDER_ACK : RESPONDER_NAK;
    arr_out[0] = svdmh_out.object;
    length_out++;
    sendVdmMessage(port, SOP_TYPE_SOP, arr_out, length_out,
                   port->PolicyState);
}

void DP_SendAttention(struct Port *port)
{
    doDataObject_t svdmh = {0};
    FSC_U32 length = 0;
    FSC_U32 arr[2] = {0};
    svdmh.SVDM.SVID = DP_SID;
    svdmh.SVDM.VDMType = STRUCTURED_VDM;
    svdmh.SVDM.Version = DPM_SVdmVer(port, SOP_TYPE_SOP);
    /* saved mode position */
    svdmh.SVDM.ObjPos = port->DisplayPortData.DpModeEntered & 0x7;
    svdmh.SVDM.CommandType = INITIATOR;
    svdmh.SVDM.Command = DP_COMMAND_ATTENTION;
    arr[0] = svdmh.object;
    length++;
    arr[1] = port->DisplayPortData.DpStatus.word;
    length++;
    sendVdmMessage(port, SOP_TYPE_SOP, arr, length,
                   port->PolicyState);
}

/**
 * The status update might be sent using attention or DP status message
 */
void DP_ProcessPartnerAttention(struct Port *port, DisplayPortStatus_t stat)
{
    DisplayPortStatus_t diff;
    /* Compare which bits has changed. Bits that are changed is set to 1 */
    diff.word =  (port->DisplayPortData.DpPpStatus.word ^ stat.word);

    if (diff.Connection)
    {
      /* Change in connection detected */
      switch(stat.Connection)
      {
      case DP_MODE_DISABLED:
        /* Connection lost by partner device*/
        if (port->DisplayPortData.DpConfigured == TRUE)
        {
          /* disable display port data if active */
        }
        break;
      case DP_MODE_DFP_D:
        /* TODO: handle DP source device present */
        if (port->DisplayPortData.DpCapMatched == TRUE &&
            port->DisplayPortData.DpCap.UfpDCapable)
        {
          port->DisplayPortData.DpPpConfig.Mode = DP_CONF_DFP_D;
          /* Select pin assignment again to make sure that capability match */
          if (DP_SelectPinAssignment(port) == TRUE)
          {
            DP_RequestPartnerConfig(port, port->DisplayPortData.DpPpConfig);
          }
        }
        break;
      case DP_MODE_UFP_D:
        /* DP sink device present */
        if (port->DisplayPortData.DpCapMatched == TRUE &&
            port->DisplayPortData.DpCap.DfpDCapable)
        {
          port->DisplayPortData.DpPpConfig.Mode = DP_CONF_UFP_D;
          if (DP_SelectPinAssignment(port) == TRUE)
          {
            DP_RequestPartnerConfig(port, port->DisplayPortData.DpPpConfig);
          }
        }
        break;
      case DP_MODE_BOTH:
        if (port->DisplayPortData.DpCapMatched)
        {
          /* Send previously selected configuration */
          DP_RequestPartnerConfig(port, port->DisplayPortData.DpPpConfig);
        }
      }
    }

    if (stat.ExitDpModeRequest)
    {
      requestExitMode(port, SOP_TYPE_SOP, DP_SID, port->DisplayPortData.DpModeEntered);
    }

    /* For all non-pd events do a callback */
    platform_dp_status_update(stat.word);
}

void DP_UpdatePartnerStatus(struct Port *port, DisplayPortStatus_t status,
                           FSC_BOOL success)
{
    /* If a device is both sink and source capable and our preferred connection
     * is not currently connected. It can be changed immediately after receiving
     * status and before sending config. For now we can wait for preferred
     * connection to be active.  */
    if (success == TRUE)
    {
      port->DisplayPortData.DpPpStatus = status;
      platform_dp_status_update(port->DisplayPortData.DpPpStatus.word);
    }
}

/**
 * The response is sent by partner device for config request
 */
void DP_ProcessConfigResponse(struct Port *port, FSC_BOOL success)
{
    if (success == TRUE)
    {
      if (port->DisplayPortData.DpPpStatus.Connection > 0)
      {
        /* If we recieve ACK and the status had reported that the connection
         * was active then we can safely start sourcing DFP_D data. This can
         * happen with UFP_D devices that report plugged in or always plugged.
         * Otherwise wait for ATTENTION and connection status.
         * do a callback to start providing data */
        /* Just set active status here */
        port->DisplayPortData.DpConfigured = TRUE;
        platform_dp_enable_pins(TRUE, port->DisplayPortData.DpPpConfig.word);
      }
    }
}

/**
 * The request is sent by partner device for config request
 */
FSC_BOOL DP_ProcessConfigRequest(struct Port *port, DisplayPortConfig_t config)
{
    return platform_dp_enable_pins(TRUE, config.word);
}
#endif /* FSC_HAVE_DP */
