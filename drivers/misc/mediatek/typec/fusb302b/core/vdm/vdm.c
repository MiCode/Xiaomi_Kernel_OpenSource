/*******************************************************************************
 * @file     vdm.c
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
#include "vdm.h"
#include "platform.h"
#include "PDPolicy.h"
#include "PD_Types.h"
#include "vdm_types.h"
#include "bitfield_translators.h"
#include "fsc_vdm_defs.h"

#ifdef FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#endif /* FSC_HAVE_DP */

/*
 * Initialize the VDM Manager (no definition/configuration object necessary).
 * returns 0 on success.
 * */
FSC_S32 initializeVdm(struct Port *port)
{
    port->vdm_timeout = FALSE;
    port->vdm_expectingresponse = FALSE;
    port->AutoModeEntryEnabled = FALSE;

#ifdef FSC_HAVE_DP
    DP_Initialize(port);
#endif /* FSC_HAVE_DP */

    return 0;
}

/* call this routine to issue Discover Identity commands
 * Discover Identity only valid for SOP/SOP'
 * returns 0 if successful
 * returns >0 if not SOP or SOP', or if Policy State is wrong
 */
FSC_S32 requestDiscoverIdentity(struct Port *port, SopType sop)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState == peSinkReady)
            || (port->PolicyState == peSourceReady))
    {
        /* different states for port partner discovery vs cable discovery */
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP)
        {
            __n_pe = peDfpUfpVdmIdentityRequest;
        }
        else if (sop == SOP_TYPE_SOP1)
        {
            __n_pe = peDfpCblVdmIdentityRequest;
        }
        else
        {
            return 1;
        }

        /* PD SID to be used for Discover Identity command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* does not matter for Discover Identity */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* discover identity command! */
        __vdmh.SVDM.Command = DISCOVER_IDENTITY;

        __arr[0] = __vdmh.object;
        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    /* allow cable discovery in special earlier states */
    else if ((sop == SOP_TYPE_SOP1) &&
              ((port->PolicyState == peSourceSendCaps)
              || (port->PolicyState == peSourceDiscovery)))
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peSrcVdmIdentityRequest;
        /* PD SID to be used for Discover Identity command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* does not matter for Discover Identity */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* discover identity command! */
        __vdmh.SVDM.Command = DISCOVER_IDENTITY;

        __arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    else
    {
        return 1;
    }

    return 0;
}

/* call this routine to issue Discover SVID commands
 * Discover SVIDs command valid with SOP*.
 */
FSC_S32 requestDiscoverSvids(struct Port *port, SopType sop)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmSvidsRequest;

        /* PD SID to be used for Discover SVIDs command */
        __vdmh.SVDM.SVID = PD_SID;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* does not matter for Discover SVIDs */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Discover SVIDs command! */
        __vdmh.SVDM.Command = DISCOVER_SVIDS;

        arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, arr, __length, __n_pe);
    }
    return 0;
}

/* call this routine to issue Discover Modes
 * Discover Modes command valid with SOP*.
 */
FSC_S32 requestDiscoverModes(struct Port *port, SopType sop, FSC_U16 svid)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    PolicyState_t __n_pe;
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        __n_pe = peDfpVdmModesRequest;

        /* Use the SVID that was discovered */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* does not matter for Discover Modes */
        __vdmh.SVDM.ObjPos = 0;
        /* we are initiating discovery */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Discover MODES command! */
        __vdmh.SVDM.Command = DISCOVER_MODES;

        __arr[0] = __vdmh.object;

        sendVdmMessageWithTimeout(port, sop, __arr, __length, __n_pe);
    }
    return 0;
}

/* DPM (UFP) calls this routine to request sending an attention command */
FSC_S32 requestSendAttention(struct Port *port, SopType sop, FSC_U16 svid,
                             FSC_U8 mode)
{
    doDataObject_t __vdmh = { 0 };
    FSC_U32 __length = 1;
    FSC_U32 __arr[__length ];
    FSC_U32 i;
    for (i = 0; i < __length; i++)
    {
        __arr[i] = 0;
    }

    port->originalPolicyState = port->PolicyState;

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        /* Use the SVID that needs attention */
        __vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        __vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        __vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* use the mode index that needs attention */
        __vdmh.SVDM.ObjPos = mode;
        /* we are initiating attention */
        __vdmh.SVDM.CommandType = INITIATOR;
        /* Attention command! */
        __vdmh.SVDM.Command = ATTENTION;

        __arr[0] = __vdmh.object;
        __length = 1;
        sendVdmMessage(port, sop, __arr, __length, port->PolicyState);
    }
    return 0;
}

FSC_S32 processDiscoverIdentity(struct Port *port, SopType sop, FSC_U32* arr_in,
                                FSC_U32 length_in)
{
    doDataObject_t vdmh_in = { 0 };
    doDataObject_t vdmh_out = { 0 };

    IdHeader idh;
    CertStatVdo csvdo;
    Identity id;
    ProductVdo pvdo;

    FSC_U32 arr[7] = { 0 };
    FSC_U32 length;
    FSC_BOOL result;

    vdmh_in.object = arr_in[0];

    /* Must NAK or not respond to Discover ID with wrong SVID */
    if (vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (vdmh_in.SVDM.CommandType == INITIATOR)
    {
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP &&
            evalResponseToSopVdm(port, vdmh_in) == TRUE)
        {
                SetPEState(port, peUfpVdmGetIdentity);
                id = port->vdmm.req_id_info(port);
                SetPEState(port, peUfpVdmSendIdentity);
        }
        else if (sop == SOP_TYPE_SOP1 &&
                 evalResponseToCblVdm(port, vdmh_in) == TRUE)
        {
                SetPEState(port, peCblGetIdentity);
                id = port->vdmm.req_id_info(port);
                if (id.nack) {
                    SetPEState(port, peCblGetIdentityNak);
                }
                else {
                    SetPEState(port, peCblSendIdentity);
                }
        }
        else
        {
            /* Message not applicable or not a valid request */
            id.nack = TRUE;
        }

        /* always use PS_SID for Discover Identity, even on response */
        vdmh_out.SVDM.SVID = PD_SID;
        /* Discovery Identity is Structured */
        vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
        /* doesn't matter for Discover Identity */
        vdmh_out.SVDM.ObjPos = 0;
        if (id.nack) {
            vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else {
            vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover Identity */
        vdmh_out.SVDM.Command = DISCOVER_IDENTITY;
        arr[0] = vdmh_out.object;
        length = 1;

        if (id.nack == FALSE)
        {
            /* put capabilities into ID Header */
            idh = id.id_header;

            /* put test ID into Cert Stat VDO Object */
            csvdo = id.cert_stat_vdo;

            arr[1] = getBitsForIdHeader(idh);
            length++;
            arr[2] = getBitsForCertStatVdo(csvdo);
            length++;

            /* Product VDO should be sent for all */
            pvdo = id.product_vdo;
            arr[length] = getBitsForProductVdo(pvdo);
            length++;

            /* Cable VDO should be sent when we are a Passive Cable or Active Cable */
            if ((idh.product_type_ufp == PASSIVE_CABLE)
                    || (idh.product_type_ufp == ACTIVE_CABLE))
            {
                CableVdo cvdo_out;
                cvdo_out = id.cable_vdo;
                arr[length] = getBitsForCableVdo(cvdo_out);
                length++;
            }

            /* AMA VDO should be sent when we are an AMA! */
            if (idh.product_type_ufp == AMA)
            {
                AmaVdo amavdo_out;
                amavdo_out = id.ama_vdo;

                arr[length] = getBitsForAmaVdo(amavdo_out);
                length++;
            }
        }

        sendVdmMessage(port, sop, arr, length, port->originalPolicyState);
        return 0;
    }
    else
    {
        /* Incoming responses, ACKs, NAKs, BUSYs */
        if ((port->PolicyState == peDfpUfpVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP))
        {
            if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peDfpUfpVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peDfpUfpVdmIdentityNaked);
                port->AutoVdmState = AUTO_VDM_DONE;
            }
        }
        else if ((port->PolicyState == peDfpCblVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP1))
        {
            if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peDfpCblVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peDfpCblVdmIdentityNaked);
            }
        }
        else if ((port->PolicyState == peSrcVdmIdentityRequest)
                && (sop == SOP_TYPE_SOP1))
        {
            if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                SetPEState(port, peSrcVdmIdentityAcked);
            }
            else
            {
                SetPEState(port, peSrcVdmIdentityNaked);
            }
        }
        else
        {
            /* TODO: something weird happened. */
            return 0;
        }

        if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
        {
            result = TRUE;
            id.id_header = getIdHeader(arr_in[1]);
            id.cert_stat_vdo = getCertStatVdo(arr_in[2]);

            if ((id.id_header.product_type_ufp == HUB) ||
                (id.id_header.product_type_ufp == PERIPHERAL) ||
                (id.id_header.product_type_ufp == AMA))
            {
                id.has_product_vdo = TRUE;
                /* !!! assuming it is before AMA VDO */
                id.product_vdo = getProductVdo(arr_in[3]);
            }

            if ((id.id_header.product_type_ufp == PASSIVE_CABLE) ||
                (id.id_header.product_type_ufp == ACTIVE_CABLE))
            {
                id.has_cable_vdo = TRUE;
                id.cable_vdo = getCableVdo(arr_in[4]);
                port->cblPresent = TRUE;
            }

            if (id.id_header.product_type_ufp == AMA)
            {
                id.has_ama_vdo = TRUE;
                /* !!! assuming it is after Product VDO */
                id.ama_vdo = getAmaVdo(arr_in[4]);
            }
        }
        else
        {
            result = FALSE;
        }

        port->vdmm.inform_id(port, result, sop, id);
        port->vdm_expectingresponse = FALSE;
        SetPEState(port, port->originalPolicyState);
        TimerDisable(&port->VdmTimer);
        port->VdmTimerStarted = FALSE;
        return 0;
    }
}

FSC_S32 processDiscoverSvids(struct Port *port, SopType sop, FSC_U32* arr_in,
                             FSC_U32 length_in)
{
    doDataObject_t vdmh_in = { 0 };
    doDataObject_t vdmh_out = { 0 };

    SvidInfo svid_info;

    FSC_U32 i;
    FSC_U16 top16;
    FSC_U16 bottom16;

    FSC_U32 arr[7] = { 0 };
    FSC_U32 length;

    vdmh_in.object = arr_in[0];

    /* Must NAK or not respond to Discover SVIDs with wrong SVID */
    if (vdmh_in.SVDM.SVID != PD_SID) return -1;

    if (vdmh_in.SVDM.CommandType == INITIATOR)
    {
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP &&
            evalResponseToSopVdm(port, vdmh_in) == TRUE)
        {
            /* assuming that the splitting of SVID info is done outside this block */
            svid_info = port->vdmm.req_svid_info(port);
            SetPEState(port, peUfpVdmGetSvids);
            SetPEState(port, peUfpVdmSendSvids);
        }
        else if (sop == SOP_TYPE_SOP1 &&
                 evalResponseToCblVdm(port, vdmh_in) == TRUE)
        {
            SetPEState(port, peCblGetSvids);
            svid_info = port->vdmm.req_svid_info(port);
            if (svid_info.nack) {
                SetPEState(port, peCblGetSvidsNak);
            }
            else {
                SetPEState(port, peCblSendSvids);
            }
        }
        else
        {
            /* Message not applicable or not a valid request */
            svid_info.nack = TRUE;
        }

        /* always use PS_SID for Discover SVIDs, even on response */
        vdmh_out.SVDM.SVID = PD_SID;
        /* Discovery SVIDs is Structured */
        vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
        /* doesn't matter for Discover SVIDs */
        vdmh_out.SVDM.ObjPos = 0;
        if (svid_info.nack) {
            vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else {
            vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover SVIDs */
        vdmh_out.SVDM.Command = DISCOVER_SVIDS;

        length = 0;
        arr[length] = vdmh_out.object;
        length++;

        if (svid_info.nack == FALSE)
        {
            FSC_U32 i;
            /* prevent segfaults */
            if (svid_info.num_svids > MAX_NUM_SVIDS)
            {
                SetPEState(port, port->originalPolicyState);
                return 1;
            }

            for (i = 0; i < svid_info.num_svids; i++)
            {
                /* check if i is even */
                if (!(i & 0x1))
                {
                    length++;

                    /* setup new word to send */
                    arr[length - 1] = 0;

                    /* if even, shift SVID up to the top 16 bits */
                    arr[length - 1] |= svid_info.svids[i];
                    arr[length - 1] <<= 16;
                }
                else
                {
                    /* if odd, fill out the bottom 16 bits */
                    arr[length - 1] |= svid_info.svids[i];
                }
            }
        }

        sendVdmMessage(port, sop, arr, length, port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        svid_info.num_svids = 0;

        if (port->PolicyState != peDfpVdmSvidsRequest)
        {
            return 1;
        }
        else if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
        {
            for (i = 1; i < length_in; i++)
            {
                top16 = (arr_in[i] >> 16) & 0x0000FFFF;
                bottom16 = (arr_in[i] >> 0) & 0x0000FFFF;

                /* if top 16 bits are 0, we're done getting SVIDs */
                if (top16 == 0x0000)
                {
                    break;
                }
                else
                {
                    svid_info.svids[2 * (i - 1)] = top16;
                    svid_info.num_svids += 1;
                }
                /* if bottom 16 bits are 0 we're done getting SVIDs */
                if (bottom16 == 0x0000)
                {
                    break;
                }
                else
                {
                    svid_info.svids[2 * (i - 1) + 1] = bottom16;
                    svid_info.num_svids += 1;
                }
            }
            SetPEState(port, peDfpVdmSvidsAcked);
        }
        else
        {
            SetPEState(port, peDfpVdmSvidsNaked);
        }

        port->vdmm.inform_svids(port, port->PolicyState == peDfpVdmSvidsAcked,
                                sop, svid_info);

        port->vdm_expectingresponse = FALSE;
        TimerDisable(&port->VdmTimer);
        port->VdmTimerStarted = FALSE;
        SetPEState(port, port->originalPolicyState);
        return 0;
    }

    return 0;
}

FSC_S32 processDiscoverModes(struct Port *port, SopType sop, FSC_U32* arr_in,
                             FSC_U32 length_in)
{
    doDataObject_t vdmh_in = { 0 };
    doDataObject_t vdmh_out = { 0 };

    ModesInfo modes_info = { 0 };

    FSC_U32 i;
    FSC_U32 arr[7] = { 0 };
    FSC_U32 length;

    vdmh_in.object = arr_in[0];
    if (vdmh_in.SVDM.CommandType == INITIATOR)
    {

        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP &&
            evalResponseToSopVdm(port, vdmh_in) == TRUE)
        {
            modes_info = port->vdmm.req_modes_info(port, vdmh_in.SVDM.SVID);
            SetPEState(port, peUfpVdmGetModes);
            SetPEState(port, peUfpVdmSendModes);
        }
        else if (sop == SOP_TYPE_SOP1 &&
                 evalResponseToCblVdm(port, vdmh_in) == TRUE)
        {
            port->originalPolicyState = port->PolicyState;
            SetPEState(port, peCblGetModes);
            modes_info = port->vdmm.req_modes_info(port, vdmh_in.SVDM.SVID);
            if (modes_info.nack) {
                SetPEState(port, peCblGetModesNak);
            }
            else {
                SetPEState(port, peCblSendModes);
            }
        }
        else
        {
            /* Message not applicable or not a valid request */
            modes_info.nack = TRUE;
        }

        /* reply with SVID we're being asked about */
        vdmh_out.SVDM.SVID = vdmh_in.SVDM.SVID;
        /* Discovery Modes is Structured */
        vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
        /* doesn't matter for Discover Modes */
        vdmh_out.SVDM.ObjPos = 0;
        if (modes_info.nack) {
            vdmh_out.SVDM.CommandType = RESPONDER_NAK;
        }
        else {
            vdmh_out.SVDM.CommandType = RESPONDER_ACK;
        }
        /* Reply with same command, Discover Modes */
        vdmh_out.SVDM.Command = DISCOVER_MODES;

        length = 0;
        arr[length] = vdmh_out.object;
        length++;

        if (modes_info.nack == FALSE)
        {
            FSC_U32 j;

            for (j = 0; j < modes_info.num_modes; j++)
            {
                arr[j + 1] = modes_info.modes[j];
                length++;
            }
        }

        sendVdmMessage(port, sop, arr, length, port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        if (port->PolicyState != peDfpVdmModesRequest)
        {
            return 1;
        }
        else
        {
            if (vdmh_in.SVDM.CommandType == RESPONDER_ACK)
            {
                modes_info.svid = vdmh_in.SVDM.SVID;
                modes_info.num_modes = length_in - 1;

                for (i = 1; i < length_in; i++)
                {
                    modes_info.modes[i - 1] = arr_in[i];
                }
                SetPEState(port, peDfpVdmModesAcked);
            }
            else
            {
                SetPEState(port, peDfpVdmModesNaked);
            }

            port->vdmm.inform_modes(port,
                                    port->PolicyState == peDfpVdmModesAcked,
                                    sop, modes_info);
            port->vdm_expectingresponse = FALSE;
            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
            SetPEState(port, port->originalPolicyState);
        }

        return 0;
    }
}

FSC_S32 processEnterMode(struct Port *port, SopType sop, FSC_U32* arr_in,
                         FSC_U32 length_in)
{
    doDataObject_t svdmh_in = { 0 };
    doDataObject_t svdmh_out = { 0 };

    FSC_BOOL mode_entered = FALSE;
    FSC_U32 arr_out[7] = { 0 };
    FSC_U32 length_out;

    svdmh_in.object = arr_in[0];
    if (svdmh_in.SVDM.CommandType == INITIATOR)
    {
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP &&
            evalResponseToSopVdm(port, svdmh_in) == TRUE)
        {
            SetPEState(port, peUfpVdmEvaluateModeEntry);
            mode_entered = port->vdmm.req_mode_entry(port,
                                                       svdmh_in.SVDM.SVID,
                                                       svdmh_in.SVDM.ObjPos);
            /* if DPM says OK, respond with ACK */
            if (mode_entered) {
                SetPEState(port, peUfpVdmModeEntryAck);
            }
            else {
                SetPEState(port, peUfpVdmModeEntryNak);
            }
        }
        else if (sop == SOP_TYPE_SOP1 &&
                 evalResponseToCblVdm(port, svdmh_in) == TRUE)
        {
            SetPEState(port, peCblEvaluateModeEntry);
            mode_entered = port->vdmm.req_mode_entry(port,
                                                       svdmh_in.SVDM.SVID,
                                                       svdmh_in.SVDM.ObjPos);
            /* if DPM says OK, respond with ACK */
            if (mode_entered) {
                SetPEState(port, peCblModeEntryAck);
            }
            else {
                SetPEState(port, peCblModeEntryNak);
            }
        }
        else
        {
            mode_entered = FALSE;
        }

        /*
         * most of the message response will be the same
         * whether we entered the mode or not
         */
        /* reply with SVID we're being asked about */
        svdmh_out.SVDM.SVID = svdmh_in.SVDM.SVID;
        /* Enter Mode is Structured */
        svdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        svdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
        /* reflect the object position */
        svdmh_out.SVDM.ObjPos = svdmh_in.SVDM.ObjPos;
        /* Reply with same command, Enter Mode */
        svdmh_out.SVDM.Command = ENTER_MODE;

        svdmh_out.SVDM.CommandType = (mode_entered == TRUE) ?
                                       RESPONDER_ACK : RESPONDER_NAK;

        arr_out[0] = svdmh_out.object;
        length_out = 1;

        sendVdmMessage(port, sop, arr_out, length_out,
                       port->originalPolicyState);
        return 0;
    }
    else
    { /* Incoming responses, ACKs, NAKs, BUSYs */
        if (svdmh_in.SVDM.CommandType != RESPONDER_ACK)
        {
            SetPEState(port, peDfpVdmModeEntryNaked);
            port->vdmm.enter_mode_result(port, FALSE, svdmh_in.SVDM.SVID,
                                         svdmh_in.SVDM.ObjPos);
        }
        else
        {
            SetPEState(port, peDfpVdmModeEntryAcked);
            port->vdmm.enter_mode_result(port, TRUE, svdmh_in.SVDM.SVID,
                                         svdmh_in.SVDM.ObjPos);
        }
        SetPEState(port, port->originalPolicyState);
        port->vdm_expectingresponse = FALSE;
        return 0;
    }
}

FSC_S32 processExitMode(struct Port *port, SopType sop, FSC_U32* arr_in,
                        FSC_U32 length_in)
{
    doDataObject_t vdmh_in = { 0 };
    doDataObject_t vdmh_out = { 0 };

    FSC_BOOL mode_exited = FALSE;
    FSC_U32 arr[7] = { 0 };
    FSC_U32 length;

    vdmh_in.object = arr_in[0];
    if (vdmh_in.SVDM.CommandType == INITIATOR)
    {
        port->originalPolicyState = port->PolicyState;
        if (sop == SOP_TYPE_SOP &&
            evalResponseToSopVdm(port, vdmh_in) == TRUE)
        {
            SetPEState(port, peUfpVdmModeExit);
            mode_exited = port->vdmm.req_mode_exit(port, vdmh_in.SVDM.SVID,
                                                     vdmh_in.SVDM.ObjPos);

            /* if DPM says OK, respond with ACK */
            if (mode_exited) {
                SetPEState(port, peUfpVdmModeExitAck);
            }
            else {
                SetPEState(port, peUfpVdmModeExitNak);
            }
        }
        else if (sop == SOP_TYPE_SOP1 &&
                evalResponseToCblVdm(port, vdmh_in))
        {
            SetPEState(port, peCblModeExit);
            mode_exited = port->vdmm.req_mode_exit(port, vdmh_in.SVDM.SVID,
                                                     vdmh_in.SVDM.ObjPos);

            /* if DPM says OK, respond with ACK */
            if (mode_exited) {
                SetPEState(port, peCblModeExitAck);
            }
            else {
                SetPEState(port, peCblModeExitNak);
            }
        }
        else
        {
            mode_exited = FALSE;
        }
        /*
         * most of the message response will be the same whether we
         * exited the mode or not
         */
        /* reply with SVID we're being asked about */
        vdmh_out.SVDM.SVID = vdmh_in.SVDM.SVID;
        /* Exit Mode is Structured */
        vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
        /* reflect the object position */
        vdmh_out.SVDM.ObjPos = vdmh_in.SVDM.ObjPos;
        /* Reply with same command, Exit Mode */
        vdmh_out.SVDM.Command = EXIT_MODE;
        vdmh_out.SVDM.CommandType = (mode_exited == TRUE) ?
                                      RESPONDER_ACK : RESPONDER_NAK;
        arr[0] = vdmh_out.object;
        length = 1;

        sendVdmMessage(port, sop, arr, length, port->originalPolicyState);
        return 0;
    }
    else
    {
        if (vdmh_in.SVDM.CommandType == RESPONDER_BUSY)
        {
            port->vdmm.exit_mode_result(port, FALSE, vdmh_in.SVDM.SVID,
                                        vdmh_in.SVDM.ObjPos);
            if (port->originalPolicyState == peSourceReady)
            {
                SetPEState(port, peSourceHardReset);
            }
            else if (port->originalPolicyState == peSinkReady)
            {
                SetPEState(port, peSinkHardReset);
            }
            else
            {
                /* TODO: should never reach here, but you never know... */
            }
        }
        else if (vdmh_in.SVDM.CommandType == RESPONDER_NAK)
        {
            port->vdmm.exit_mode_result(port, FALSE, vdmh_in.SVDM.SVID,
                                        vdmh_in.SVDM.ObjPos);
            SetPEState(port, port->originalPolicyState);

            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
        }
        else
        {
            SetPEState(port, peDfpVdmExitModeAcked);
            port->vdmm.exit_mode_result(port, TRUE, vdmh_in.SVDM.SVID,
                                        vdmh_in.SVDM.ObjPos);
            SetPEState(port, port->originalPolicyState);

            TimerDisable(&port->VdmTimer);
            port->VdmTimerStarted = FALSE;
        }
        port->vdm_expectingresponse = FALSE;
        return 0;
    }
}

FSC_S32 processAttention(struct Port *port, SopType sop, FSC_U32* arr_in,
                         FSC_U32 length_in)
{
    doDataObject_t vdmh_in = { 0 };
    vdmh_in.object = arr_in[0];

    port->originalPolicyState = port->PolicyState;
    SetPEState(port, peDfpVdmAttentionRequest);
    SetPEState(port, port->originalPolicyState);

#ifdef FSC_HAVE_DP
    if (vdmh_in.SVDM.SVID == DP_SID) {
        DP_ProcessCommand(port, arr_in);
    }
    else
#endif /* FSC_HAVE_DP */
    {
        port->vdmm.inform_attention(port, vdmh_in.SVDM.SVID,
                                    vdmh_in.SVDM.ObjPos);
    }
    return 0;
}

FSC_S32 processSvidSpecific(struct Port *port, SopType sop, FSC_U32* arr_in,
                            FSC_U32 length_in)
{
    doDataObject_t vdmh_out = { 0 };
    doDataObject_t vdmh_in = { 0 };
    FSC_U32 arr[7] = { 0 };
    FSC_U32 length;

    vdmh_in.object = arr_in[0];

#ifdef FSC_HAVE_DP
    if (vdmh_in.SVDM.SVID == DP_SID)
    {
        if (!DP_ProcessCommand(port, arr_in))
        {
            return 0; /* DP code will send response, so return */
        }
    }
#endif /* FSC_HAVE_DP */

    /* in this case the command is unrecognized. Reply with a NAK. */
    /* reply with SVID we received */
    vdmh_out.SVDM.SVID = vdmh_in.SVDM.SVID;
    /* All are structured in this switch-case */
    vdmh_out.SVDM.VDMType = STRUCTURED_VDM;
    /* version 1.0 or 2.0 */
    vdmh_out.SVDM.Version = DPM_SVdmVer(port, sop);
    /* value doesn't matter here */
    vdmh_out.SVDM.ObjPos = 0;
    /* Command unrecognized, so NAK */
    vdmh_out.SVDM.CommandType = RESPONDER_NAK;
    /* Reply with same command */
    vdmh_out.SVDM.Command = vdmh_in.SVDM.Command;

    arr[0] = vdmh_out.object;
    length = 1;

    sendVdmMessage(port, sop, arr, length, port->originalPolicyState);
    return 0;
}

/**
 * Determine message applicability or whether to a response is required
 */
FSC_BOOL evalResponseToSopVdm(struct Port *port, doDataObject_t vdm_hdr)
{
    FSC_BOOL response = TRUE;

    if (port->PolicyIsDFP == TRUE && !Responds_To_Discov_SOP_DFP) {
        response = FALSE;
    }
    else if (port->PolicyIsDFP == FALSE && !Responds_To_Discov_SOP_UFP) {
        response = FALSE;
    }
    else if (DPM_SpecRev(port, SOP_TYPE_SOP) < USBPDSPECREV3p0 &&
             port->PolicyIsDFP == TRUE) {
        /* See message applicability */
        response = FALSE;
    }
    else if (!(port->PolicyState == peSourceReady ||
               port->PolicyState == peSinkReady)) {
        /* Neither sink ready or source ready state */
        response = FALSE;
    }
    return response;
}

/**
 * Determine message applicability or whether to a response is required
 */
FSC_BOOL evalResponseToCblVdm(struct Port *port, doDataObject_t vdm_hdr)
{
    FSC_BOOL response = TRUE;
    if (port->PolicyState != peCblReady) {
        response = FALSE;
    }
    return response;
}

/**
 * Determine message applicability or whether to a response is required
 */


/* returns 0 on success, 1+ otherwise */
FSC_S32 processVdmMessage(struct Port *port, SopType sop, FSC_U32* arr_in,
                          FSC_U32 length_in)
{
    FSC_S32 result = 0;
    doDataObject_t vdmh_in = { 0 };

    vdmh_in.object = arr_in[0];

    if (vdmh_in.SVDM.VDMType == STRUCTURED_VDM)
    {
        /* Discover ID for SOP'/" will update cable PD spec revision */
        if (vdmh_in.SVDM.Command == DISCOVER_IDENTITY &&
            (sop == SOP_TYPE_SOP1 || sop == SOP_TYPE_SOP2))
        {
            DPM_SetSpecRev(port, sop, port->PolicyRxHeader.SpecRevision);
        }

        if (DPM_SVdmVer(port, sop) != vdmh_in.SVDM.Version)
        {
            /* Invalid structured VDM version */
            return 1;
        }

        // different actions for different commands
        switch (vdmh_in.SVDM.Command)
        {
        case DISCOVER_IDENTITY:
            result = processDiscoverIdentity(port, sop, arr_in, length_in);
            break;
        case DISCOVER_SVIDS:
            result = processDiscoverSvids(port, sop, arr_in, length_in);
            break;
        case DISCOVER_MODES:
            result = processDiscoverModes(port, sop, arr_in, length_in);
            break;
        case ENTER_MODE:
            result = processEnterMode(port, sop, arr_in, length_in);
            break;
        case EXIT_MODE:
            result = processExitMode(port, sop, arr_in, length_in);
            break;
        case ATTENTION:
            result = processAttention(port, sop, arr_in, length_in);
            break;
        default:
            // SVID-Specific commands go here
            result = processSvidSpecific(port, sop, arr_in, length_in);
            break;
        }

        return result;
    }
    else
    {
        /* TODO: Unstructured messages */
        /* Unstructured VDM's not supported at this time */
        if (DPM_SpecRev(port, sop) == USBPDSPECREV3p0)
        {
            /* Not supported in PD3.0, ignored in PD2.0 */
            SetPEState(port, peNotSupported);
        }
        return 1;
    }
}

/* call this function to enter a mode */
FSC_S32 requestEnterMode(struct Port *port, SopType sop, FSC_U16 svid,
                         FSC_U32 mode_index)
{
    doDataObject_t vdmh = { 0 };
    FSC_U32 length = 1;
    FSC_U32 arr[length ];
    PolicyState_t n_pe;
    FSC_U32 i;
    for (i = 0; i < length; i++)
    {
        arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        n_pe = peDfpVdmModeEntryRequest;

        /* Use SVID specified upon function call */
        vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* select mode */
        vdmh.SVDM.ObjPos = mode_index;
        /* we are initiating mode entering */
        vdmh.SVDM.CommandType = INITIATOR;
        /* Enter Mode command! */
        vdmh.SVDM.Command = ENTER_MODE;

        arr[0] = vdmh.object;
        sendVdmMessageWithTimeout(port, sop, arr, length, n_pe);
    }
    return 0;
}

/* call this function to exit a mode */
FSC_S32 requestExitMode(struct Port *port, SopType sop, FSC_U16 svid,
                        FSC_U32 mode_index)
{
    doDataObject_t vdmh = { 0 };
    FSC_U32 length = 1;
    FSC_U32 arr[length ];
    PolicyState_t n_pe;
    FSC_U32 i;
    for (i = 0; i < length; i++)
    {
        arr[i] = 0;
    }

    if ((port->PolicyState != peSinkReady)
            && (port->PolicyState != peSourceReady))
    {
        return 1;
    }
    else
    {
        port->originalPolicyState = port->PolicyState;
        n_pe = peDfpVdmModeExitRequest;

        /* Use SVID specified upon function call */
        vdmh.SVDM.SVID = svid;
        /* structured VDM Header */
        vdmh.SVDM.VDMType = STRUCTURED_VDM;
        /* version 1.0 or 2.0 */
        vdmh.SVDM.Version = DPM_SVdmVer(port, sop);
        /* select mode */
        vdmh.SVDM.ObjPos = mode_index;
        /* we are initiating mode entering */
        vdmh.SVDM.CommandType = INITIATOR;
        /* Exit Mode command! */
        vdmh.SVDM.Command = EXIT_MODE;

        arr[0] = vdmh.object;
        sendVdmMessageWithTimeout(port, sop, arr, length, n_pe);
    }
    return 0;
}

/* call this function to send an attention command to specified SOP type */
FSC_S32 sendAttention(SopType sop, FSC_U32 obj_pos)
{
    // TODO
    return 1;
}

void sendVdmMessageWithTimeout(struct Port *port, SopType sop, FSC_U32* arr,
                               FSC_U32 length, FSC_S32 n_pe)
{
    sendVdmMessage(port, sop, arr, length, n_pe);
    port->vdm_expectingresponse = TRUE;
}

void startVdmTimer(struct Port *port, FSC_S32 n_pe)
{
    /* start the appropriate timer */
    switch (n_pe)
    {
    case peDfpUfpVdmIdentityRequest:
    case peDfpCblVdmIdentityRequest:
    case peSrcVdmIdentityRequest:
    case peDfpVdmSvidsRequest:
    case peDfpVdmModesRequest:
        TimerStart(&port->VdmTimer, tVDMSenderResponse);
        port->VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeEntryRequest:
        TimerStart(&port->VdmTimer, tVDMWaitModeEntry);
        port->VdmTimerStarted = TRUE;
        break;
    case peDfpVdmModeExitRequest:
        TimerStart(&port->VdmTimer, tVDMWaitModeExit);
        port->VdmTimerStarted = TRUE;
        break;
    case peDpRequestStatus:
    case peDpRequestConfig:
        TimerStart(&port->VdmTimer, tVDMSenderResponse);
        port->VdmTimerStarted = TRUE;
        break;
    default:
        TimerDisable(&port->VdmTimer);
        /* timeout immediately */
        port->VdmTimerStarted = TRUE;
        break;
    }
}

void sendVdmMessageFailed(struct Port *port)
{
    resetPolicyState(port);
}

/* call this function when VDM Message Timer expires */
void vdmMessageTimeout(struct Port *port)
{
    resetPolicyState(port);
}

void resetPolicyState(struct Port *port)
{
    /* fake empty id Discover Identity for NAKs */
    Identity __id = { 0 };
    SvidInfo __svid_info = { 0 };
    ModesInfo __modes_info = { 0 };

    port->vdm_expectingresponse = FALSE;
    port->VdmTimerStarted = FALSE;
    TimerDisable(&port->VdmTimer);

    if (port->PolicyState == peGiveVdm)
    {
        SetPEState(port, port->vdm_next_ps);
    }

    if (port->VdmMsgTxSop == SOP_TYPE_SOP1 &&
        port->cblPresent)
    {
        port->cblRstState = CBL_RST_START;
    }

    switch (port->PolicyState)
    {
    case peDfpUfpVdmIdentityRequest:
        SetPEState(port, peDfpUfpVdmIdentityNaked);
        /* informing of a NAK */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpCblVdmIdentityRequest:
        SetPEState(port, peDfpCblVdmIdentityNaked);
        /* informing of a NAK from cable */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP1, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmSvidsRequest:
        SetPEState(port, peDfpVdmSvidsNaked);
        port->vdmm.inform_svids(port, FALSE, SOP_TYPE_SOP, __svid_info);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModesRequest:
        SetPEState(port, peDfpVdmModesNaked);
        port->vdmm.inform_modes(port, FALSE, SOP_TYPE_SOP, __modes_info);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModeEntryRequest:
        SetPEState(port, peDfpVdmModeEntryNaked);
        port->vdmm.enter_mode_result(port, FALSE, 0, 0);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDfpVdmModeExitRequest:
        port->vdmm.exit_mode_result(port, FALSE, 0, 0);

        /* if Mode Exit request is NAKed, go to hard reset state! */
        if (port->originalPolicyState == peSinkReady)
        {
            SetPEState(port, peSinkHardReset);
        }
        else if (port->originalPolicyState == peSourceReady)
        {
            SetPEState(port, peSourceHardReset);
        }
        else
        {
            // TODO: should never reach here, but...
        }
        SetPEState(port, port->originalPolicyState);
        return;
    case peSrcVdmIdentityRequest:
        SetPEState(port, peSrcVdmIdentityNaked);
        /* informing of a NAK from cable */
        port->vdmm.inform_id(port, FALSE, SOP_TYPE_SOP1, __id);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDpRequestStatus:
        SetPEState(port, peDpRequestStatusNak);
        SetPEState(port, port->originalPolicyState);
        break;
    case peDpRequestConfig:
        SetPEState(port, peDpRequestStatusNak);
        SetPEState(port, port->originalPolicyState);
        break;
    default:
        SetPEState(port, port->originalPolicyState);
        break;
    }
}

FSC_BOOL evaluateModeEntry(struct Port *port, FSC_U32 mode_in)
{
    return (mode_in == MODE_AUTO_ENTRY && port->AutoModeEntryEnabled) ? TRUE : FALSE;
}

#endif // FSC_HAVE_VDM
