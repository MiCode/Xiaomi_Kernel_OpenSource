/*******************************************************************************
 * @file     bitfield_translators.h
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
#ifndef __VDM_BITFIELD_TRANSLATORS_H__
#define __VDM_BITFIELD_TRANSLATORS_H__

#include "platform.h"

#ifdef FSC_HAVE_VDM
/*
 * Functions that convert bits into internal header representations...
 */
UnstructuredVdmHeader getUnstructuredVdmHeader(FSC_U32 in);
StructuredVdmHeader getStructuredVdmHeader(FSC_U32 in);
IdHeader getIdHeader(FSC_U32 in);
VdmType getVdmTypeOf(FSC_U32 in);
/*
 * Functions that convert internal header representations into bits...
 */
FSC_U32 getBitsForUnstructuredVdmHeader(UnstructuredVdmHeader in);
FSC_U32 getBitsForStructuredVdmHeader(StructuredVdmHeader in);
FSC_U32 getBitsForIdHeader(IdHeader in);

/*
 * Functions that convert bits into internal VDO representations...
 */
CertStatVdo getCertStatVdo(FSC_U32 in);
ProductVdo getProductVdo(FSC_U32 in);
CableVdo getCableVdo(FSC_U32 in);
AmaVdo getAmaVdo(FSC_U32 in);

/*
 * Functions that convert internal VDO representations into bits...
 */
FSC_U32 getBitsForProductVdo(ProductVdo in);
FSC_U32 getBitsForCertStatVdo(CertStatVdo in);
FSC_U32 getBitsForCableVdo(CableVdo in);
FSC_U32 getBitsForAmaVdo(AmaVdo in);

#endif /* __VDM_BITFIELD_TRANSLATORS_H__ */ // header guard

#endif /* FSC_HAVE_VDM */
