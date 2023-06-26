/*******************************************************************************
 * @file     bitfield_translators.c
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
#include "vdm_types.h"
#include "bitfield_translators.h"

#ifdef FSC_HAVE_VDM

/* determines whether a VDM is structured or unstructured. */
VdmType getVdmTypeOf(FSC_U32 in)
{
    return getUnstructuredVdmHeader(in).vdm_type;
}

/*
 * process a 32 bit field and parses it for VDM Header data
 * returns Unstructured VDM Header struct
 */
UnstructuredVdmHeader getUnstructuredVdmHeader(FSC_U32 in)
{
    UnstructuredVdmHeader ret;

    ret.svid = (in >> 16) & 0x0000FFFF;
    ret.vdm_type = (in >> 15) & 0x00000001;
    ret.info = (in >> 0) & 0x00007FFF;

    return ret;
}

/* Turn the internal Unstructured VDM Header struct into a 32 bit field */
FSC_U32 getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in)
{
    FSC_U32 ret;

    /* Cast each member to a 32 bit type first so it can be easily ORd in */
    FSC_U32 tmp;

    ret = 0;

    tmp = in.svid;
    ret |= (tmp << 16);

    tmp = in.vdm_type;
    ret |= (tmp << 15);

    tmp = (in.info << 0);
    ret |= tmp;

    return ret;
}

/*
 * process a 32 bit field and return a parsed StructuredVdmHeader
 * assumes that the 32 bits are actually structured and not unstructured.
 * returns parsed Structured VDM Header Struct
 */
StructuredVdmHeader getStructuredVdmHeader(FSC_U32 in)
{
    StructuredVdmHeader ret;

    ret.svid = (in >> 16) & 0x0000FFFF;
    ret.vdm_type = (in >> 15) & 0x00000001;
    ret.svdm_version = (in >> 13) & 0x00000003;
    ret.obj_pos = (in >> 8) & 0x00000007;
    ret.cmd_type = (in >> 6) & 0x00000003;
    ret.command = (in >> 0) & 0x0000001F;

    return ret;
}

/* Turn the internal Structured VDM Header struct  into a 32 bit field */
FSC_U32 getBitsForStructuredVdmHeader(StructuredVdmHeader in)
{
    FSC_U32 ret;

    /* Cast each member to a 32 bit type first so it can be easily ORd in */
    FSC_U32 tmp;

    ret = 0;

    tmp = in.svid;
    ret |= (tmp << 16);

    tmp = in.vdm_type;
    ret |= (tmp << 15);

    tmp = in.svdm_version;
    ret |= (tmp << 13);

    tmp = in.obj_pos;
    ret |= (tmp << 8);

    tmp = in.cmd_type;
    ret |= (tmp << 6);

    tmp = in.command;
    ret |= (tmp << 0);

    return ret;
}

/* Process a 32 bit field and parses it for ID Header data
 * returns parsed ID Header struct
 */
IdHeader getIdHeader(FSC_U32 in)
{
    IdHeader ret;

    ret.usb_host_data_capable = (in >> 31) & 0x00000001;
    ret.usb_device_data_capable = (in >> 30) & 0x00000001;
    ret.product_type_ufp = (in >> 27) & 0x00000007;
    ret.product_type_dfp = (in >> 23) & 0x7U;
    ret.modal_op_supported = (in >> 26) & 0x00000001;
    ret.usb_vid = (in >> 0) & 0x0000FFFF;

    return ret;
}

/* Turn the internal Structured VDM Header struct into a 32 bit field */
FSC_U32 getBitsForIdHeader(IdHeader in)
{
    FSC_U32 ret;

    /* Cast each member to a 32 bit type first so it can be easily ORd in */
    FSC_U32 tmp;

    ret = 0;

    tmp = in.usb_host_data_capable;
    ret |= (tmp << 31);

    tmp = in.usb_device_data_capable;
    ret |= (tmp << 30);

    tmp = in.product_type_ufp;
    ret |= (tmp << 27);

    tmp = in.product_type_dfp;
    ret |= (tmp << 23);

    tmp = in.modal_op_supported;
    ret |= (tmp << 26);

    tmp = in.usb_vid;
    ret |= (tmp << 0);

    return ret;
}

/* process a 32 bit field and parses it for Product VDO data
 * returns parsed Product VDO struct
 */
ProductVdo getProductVdo(FSC_U32 in)
{
    ProductVdo ret;

    ret.usb_product_id = (in >> 16) & 0x0000FFFF;
    ret.bcd_device = (in >> 0) & 0x0000FFFF;

    return ret;
}

/* Turn the internal Product VDO struct representation into a 32 bit field */
FSC_U32 getBitsForProductVdo(ProductVdo in)
{
    FSC_U32 ret;
    FSC_U32 tmp;

    ret = 0;

    tmp = in.usb_product_id;
    ret |= (tmp << 16);

    tmp = in.bcd_device;
    ret |= (tmp << 0);

    return ret;
}

/*
 * process a 32 bit field and parses it for Cert Stat VDO data
 * returns parsed Cert Stat VDO struct
 */
CertStatVdo getCertStatVdo(FSC_U32 in)
{
    CertStatVdo ret;

    ret.test_id = (in >> 0) & 0x000FFFFF;

    return ret;
}

/* Turn the internal Cert Stat VDO struct representation into a 32 bit field */
FSC_U32 getBitsForCertStatVdo(CertStatVdo in)
{
    FSC_U32 ret;
    FSC_U32 tmp;

    ret = 0;

    tmp = in.test_id;
    ret |= (tmp << 0);

    return ret;
}

/*
 * process a 32 bit field and parses it for Cable VDO data
 * returns parsed Cable VDO struct
 */
CableVdo getCableVdo(FSC_U32 in)
{
    CableVdo ret;

    ret.cable_hw_version = (in >> 28) & 0x0000000F;
    ret.cable_fw_version = (in >> 24) & 0x0000000F;
    ret.cable_to_type = (in >> 18) & 0x00000003;
    ret.cable_to_pr = (in >> 17) & 0x00000001;
    ret.cable_latency = (in >> 13) & 0x0000000F;
    ret.cable_term = (in >> 11) & 0x00000003;
    ret.sstx1_dir_supp = (in >> 10) & 0x00000001;
    ret.sstx2_dir_supp = (in >> 9) & 0x00000001;
    ret.ssrx1_dir_supp = (in >> 8) & 0x00000001;
    ret.ssrx2_dir_supp = (in >> 7) & 0x00000001;
    ret.vbus_current_handling_cap = (in >> 5) & 0x00000003;
    ret.vbus_thru_cable = (in >> 4) & 0x00000001;
    ret.sop2_presence = (in >> 3) & 0x00000001;
    ret.usb_ss_supp = (in >> 0) & 0x00000007;

    return ret;
}

/* turn the internal Cable VDO representation into a 32 bit field */
FSC_U32 getBitsForCableVdo(CableVdo in)
{
    FSC_U32 ret;
    FSC_U32 tmp;

    ret = 0;

    tmp = in.cable_hw_version;
    ret |= (tmp << 28);

    tmp = in.cable_fw_version;
    ret |= (tmp << 24);

    tmp = in.cable_to_type;
    ret |= (tmp << 18);

    tmp = in.cable_to_pr;
    ret |= (tmp << 17);

    tmp = in.cable_latency;
    ret |= (tmp << 13);

    tmp = in.cable_term;
    ret |= (tmp << 11);

    tmp = in.sstx1_dir_supp;
    ret |= (tmp << 10);

    tmp = in.sstx2_dir_supp;
    ret |= (tmp << 9);

    tmp = in.ssrx1_dir_supp;
    ret |= (tmp << 8);

    tmp = in.ssrx2_dir_supp;
    ret |= (tmp << 7);

    tmp = in.vbus_current_handling_cap;
    ret |= (tmp << 5);

    tmp = in.vbus_thru_cable;
    ret |= (tmp << 4);

    tmp = in.sop2_presence;
    ret |= (tmp << 3);

    tmp = in.usb_ss_supp;
    ret |= (tmp << 0);

    return ret;
}

/*
 * process a 32 bit field and parses it for AMA VDO data
 * returns parsed AMA VDO struct
 */
AmaVdo getAmaVdo(FSC_U32 in)
{
    AmaVdo ret;

    ret.cable_hw_version = (in >> 28) & 0x0000000F;
    ret.cable_fw_version = (in >> 24) & 0x0000000F;
    ret.vdo_version = (in >> 21) & 0x00000007;
    ret.vconn_full_power = (in >> 5) & 0x00000007;
    ret.vconn_requirement = (in >> 4) & 0x00000001;
    ret.vbus_requirement = (in >> 3) & 0x00000001;
    ret.usb_ss_supp = (in >> 0) & 0x00000007;

    return ret;
}

/* turn the internal AMA VDO representation into a 32 bit field */
FSC_U32 getBitsForAmaVdo(AmaVdo in)
{
    FSC_U32 ret;
    FSC_U32 tmp;

    ret = 0;

    tmp = in.cable_hw_version;
    ret |= (tmp << 28);

    tmp = in.cable_fw_version;
    ret |= (tmp << 24);

    tmp = in.vdo_version;
    ret |= (tmp << 21);

    tmp = in.vconn_full_power;
    ret |= (tmp << 5);

    tmp = in.vconn_requirement;
    ret |= (tmp << 4);

    tmp = in.vbus_requirement;
    ret |= (tmp << 3);

    tmp = in.usb_ss_supp;
    ret |= (tmp << 0);

    return ret;
}

#endif /* FSC_HAVE_VDM */
