/*COPYRIGHT**
// -------------------------------------------------------------------------
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of the accompanying license
//  agreement or nondisclosure agreement with Intel Corporation and may not
//  be copied or disclosed except in accordance with the terms< of that
//  agreement.
//        Copyright (C) 2012-2014 Intel Corporation. All Rights Reserved.
// -------------------------------------------------------------------------
**COPYRIGHT*/


#ifndef _VALLEYVIEW_SOCHAP_H_INC_
#define _VALLEYVIEW_SOCHAP_H_INC_

/*
 * Local to this architecture: Valleyview uncore SA unit
 *
 */
#define VLV_VISA_DESKTOP_DID                 0x000C04
#define VLV_VISA_NEXT_ADDR_OFFSET            4
#define VLV_VISA_BAR_ADDR_SHIFT              32
#define VLV_VISA_BAR_ADDR_MASK               0x000FFFC00000LL
#define VLV_VISA_MAX_PCI_DEVICES             16
#define VLV_VISA_MCR_REG_OFFSET              0xD0
#define VLV_VISA_MDR_REG_OFFSET              0xD4
#define VLV_VISA_MCRX_REG_OFFSET             0xD8
#define VLV_VISA_BYTE_ENABLES                0xF
#define VLV_VISA_OP_CODE_SHIFT               24
#define VLV_VISA_PORT_ID_SHIFT               16
#define VLV_VISA_OFFSET_HI_MASK              0xFFFFFF00
#define VLV_VISA_OFFSET_LO_MASK              0xFF
#define VLV_CHAP_SIDEBAND_PORT_ID            23
#define VLV_CHAP_SIDEBAND_WRITE_OP_CODE      1
#define VLV_CHAP_SIDEBAND_READ_OP_CODE       0
#define VLV_CHAP_MAX_COUNTERS                8
#define VLV_CHAP_MAX_COUNT                   0x00000000FFFFFFFFLL

#define VLV_VISA_OTHER_BAR_MMIO_PAGE_SIZE      4096
#define VLV_VISA_CHAP_SAMPLE_DATA              0x00020000
#define VLV_VISA_CHAP_STOP                     0x00040000
#define VLV_VISA_CHAP_START                    0x00110000
#define VLV_VISA_CHAP_CTRL_REG_OFFSET          0x0


extern DISPATCH_NODE  valleyview_visa_dispatch;

#endif
