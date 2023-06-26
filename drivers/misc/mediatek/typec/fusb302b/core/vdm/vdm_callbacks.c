/*******************************************************************************
 * @file     vdm_callbacks.c
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
#include "vdm.h"
#include "vdm_callbacks.h"
#include "vdm_types.h"

#ifdef FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#endif /* FSC_HAVE_DP */

Identity vdmRequestIdentityInfo(struct Port *port)
{
    Identity id = { 0 };

    id.nack = FALSE;

    id.id_header.usb_host_data_capable = Data_Capable_as_USB_Host_SOP;
    id.id_header.usb_device_data_capable = Data_Capable_as_USB_Device_SOP;
    id.id_header.product_type_ufp = Product_Type_UFP_SOP;
    if (DPM_SVdmVer(port, SOP_TYPE_SOP) == V2P0) {
        id.id_header.product_type_dfp = Product_Type_DFP_SOP;
    }
    id.id_header.modal_op_supported = Modal_Operation_Supported_SOP;
    id.id_header.usb_vid = USB_VID_SOP;

    id.cert_stat_vdo.test_id = XID_SOP;

    id.product_vdo.usb_product_id = PID_SOP;
    id.product_vdo.bcd_device = bcdDevice_SOP;

    /* Cable VDO */
    /* Not Currently Implemented */

    /* AMA */
    id.ama_vdo.cable_hw_version = AMA_HW_Vers;
    id.ama_vdo.cable_fw_version = AMA_FW_Vers;
    id.ama_vdo.vconn_full_power = AMA_VCONN_power;
    id.ama_vdo.vconn_requirement = AMA_VCONN_reqd;
    id.ama_vdo.vbus_requirement = AMA_VBUS_reqd;
    id.ama_vdo.usb_ss_supp = AMA_Superspeed_Support;
    id.ama_vdo.vdo_version = 0x0;

    return id;
}

SvidInfo vdmRequestSvidInfo(struct Port *port)
{
    SvidInfo svid_info = { 0 };

    if (port->svid_enable &&
        Modal_Operation_Supported_SOP)
    {
        svid_info.nack = FALSE;
        svid_info.num_svids = Num_SVIDs_min_SOP;
        svid_info.svids[0] = port->my_svid;
    }
    else
    {
        svid_info.nack = TRUE;
        svid_info.num_svids = 0;
        svid_info.svids[0] = 0x0000;
    }

    return svid_info;
}

ModesInfo vdmRequestModesInfo(struct Port *port, FSC_U16 svid)
{
    ModesInfo modes_info = { 0 };

    if (port->svid_enable &&
        port->mode_enable &&
        (svid == port->my_svid))
    {
        modes_info.nack = FALSE;
        modes_info.svid = svid;
        modes_info.num_modes = 1;
#ifdef FSC_HAVE_DP
        if (svid == DP_SID)
        {
            modes_info.modes[0] = port->DisplayPortData.DpCap.word;
        }
        else
#endif /* FSC_HAVE_DP */
        {
            modes_info.modes[0] = port->my_mode;
        }
    }
    else
    {
        modes_info.nack = TRUE;
        modes_info.svid = svid;
        modes_info.num_modes = 0;
        modes_info.modes[0] = 0;
    }
    return modes_info;
}

FSC_BOOL vdmModeEntryRequest(struct Port *port, FSC_U16 svid, FSC_U32 mode_index)
{
    if (SVID1_mode1_enter_SOP &&
        port->svid_enable &&
        port->mode_enable &&
        (svid == port->my_svid) &&
        (mode_index == 1))
    {
        port->mode_entered = TRUE;

#ifdef FSC_HAVE_DP
        if (port->my_svid == DP_SID)
        {
            port->DisplayPortData.DpModeEntered = mode_index;
        }
#endif /* FSC_HAVE_DP */
        return TRUE;
    }

    return FALSE;
}

FSC_BOOL vdmModeExitRequest(struct Port *port, FSC_U16 svid, FSC_U32 mode_index)
{
    if (port->mode_entered &&
       (svid == port->my_svid) &&
       (mode_index == 1))
    {
        port->mode_entered = FALSE;

#ifdef FSC_HAVE_DP
        if (port->DisplayPortData.DpModeEntered &&
            (port->DisplayPortData.DpModeEntered == mode_index) &&
            (svid == DP_SID))
        {
            port->DisplayPortData.DpModeEntered = 0;
            platform_dp_enable_pins(FALSE, 0);
        }
#endif /* FSC_HAVE_DP */

        return TRUE;
    }
    return FALSE;
}

FSC_BOOL vdmEnterModeResult(struct Port *port, FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index)
{

    if (port->AutoModeEntryObjPos > 0)
    {
        port->AutoModeEntryObjPos = 0;
    }

    if (success)
    {
        port->mode_entered = TRUE;
#ifdef FSC_HAVE_DP
        if (svid == DP_SID)
        {
                port->DisplayPortData.DpModeEntered = mode_index;
        }
#endif /* FSC_HAVE_DP */
    }
    return TRUE;
}

void vdmExitModeResult(struct Port *port, FSC_BOOL success, FSC_U16 svid,
                       FSC_U32 mode_index)
{
    port->mode_entered = FALSE;

#ifdef FSC_HAVE_DP
    if (svid == DP_SID && port->DisplayPortData.DpModeEntered == mode_index)
    {
        port->DisplayPortData.DpModeEntered = 0;
    }
#endif /* FSC_HAVE_DP */
}

void vdmInformIdentity(struct Port *port, FSC_BOOL success, SopType sop, Identity id)
{
    /* example of checking cable current handling capability: */
    if (success && (sop == SOP_TYPE_SOP1)
            && (id.id_header.product_type_ufp == ACTIVE_CABLE))
    {
        switch (id.cable_vdo.vbus_current_handling_cap)
        {
        case VBUS_5A:
            /* support for 5A cable */
            break;
        case VBUS_3A:
            /* support for 3A cable */
            break;
        default:
            /* error case */
            break;
        }
    }
}

void vdmInformSvids(struct Port *port, FSC_BOOL success, SopType sop,
                    SvidInfo svid_info)
{
    FSC_U32 i;
	/* Reset the known index */
	port->svid_discvry_idx = -1;
	/* Assume we are goint to be done */
	port->svid_discvry_done = TRUE;

    if (success == TRUE)
    {
        port->core_svid_info.num_svids = svid_info.num_svids;
        for (i = 0; (i < svid_info.num_svids) && (i < MAX_NUM_SVIDS); i++)
        {
            port->core_svid_info.svids[i] = svid_info.svids[i];
            /* TODO: Check for correct svid here */
            if (port->core_svid_info.svids[i] == SVID1_SOP)
            {
                port->svid_discvry_idx = i;
                break;
            }
        }

        if (port->svid_discvry_idx < 0 &&
            port->core_svid_info.num_svids >= MAX_NUM_SVIDS)
        {
            /* Continue discovery as no known svid are found and there are more */
            port->svid_discvry_done = FALSE;
        }
    }
}

void vdmInformModes(struct Port *port, FSC_BOOL success, SopType sop,
                    ModesInfo modes_info)
{

    FSC_U32 i;
    port->AutoModeEntryObjPos = -1;

    if (!success) { return; }

    if (modes_info.nack == FALSE)
    {
        for (i = 0; i < modes_info.num_modes; i++)
        {
#ifdef FSC_HAVE_DP
            /* Evaluate DP mode first if defined. */
            if (modes_info.svid == DP_SID)
            {
                if (DP_EvaluateSinkCapability(port, modes_info.modes[i]))
                {
                    port->AutoModeEntryObjPos = i + 1;
                    break;
                }
            }
            else
#endif /* FSC_HAVE_DP */
            {
                if (modes_info.svid != SVID_AUTO_ENTRY) { break; }
                if (evaluateModeEntry(port, modes_info.modes[i]))
                {
                    port->AutoModeEntryObjPos = i + 1;
                    break;
                }
            }
        }
    }
}

void vdmInformAttention(struct Port *port, FSC_U16 svid, FSC_U8 mode_index)
{

}

void vdmInitDpm(struct Port *port) {

    port->svid_enable = TRUE;
    port->mode_enable = TRUE;

    port->my_svid = SVID_DEFAULT;
    port->my_mode = MODE_DEFAULT;

    port->mode_entered = FALSE;
}

#endif /* FSC_HAVE_VDM */
