/*COPYRIGHT**
    Copyright (C) 2013-2014 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/

#ifndef _UNC_COMMON_H_INC_
#define _UNC_COMMON_H_INC_


#define DRV_IS_PCI_VENDOR_ID_INTEL            0x8086
#define VENDOR_ID_MASK                        0x0000FFFF
#define DEVICE_ID_MASK                        0xFFFF0000
#define DEVICE_ID_BITSHIFT                    16
 
#define UNCORE_SOCKETID_UBOX_LNID_OFFSET      0x40
#define UNCORE_SOCKETID_UBOX_GID_OFFSET       0x54

extern U32 *unc_package_to_bus_map;

typedef struct DEVICE_CALLBACK_NODE_S  DEVICE_CALLBACK_NODE;
typedef        DEVICE_CALLBACK_NODE   *DEVICE_CALLBACK;
 
struct DEVICE_CALLBACK_NODE_S {
    DRV_BOOL (*is_Valid_Device)(U32);
    DRV_BOOL (*is_Valid_For_Write)(U32, U32);
    DRV_BOOL (*is_Unit_Ctl)(U32);
    DRV_BOOL (*is_PMON_Ctl)(U32);
};

extern VOID 
UNC_COMMON_Dummy_Func(
    PVOID param
);


extern VOID
UNC_COMMON_Do_Bus_to_Socket_Map(
    U32 socketid_ubox_did
);



/************************************************************/
/*
 * UNC common PCI  based API
 *
 ************************************************************/

extern VOID
UNC_COMMON_PCI_Write_PMU (
    PVOID            param,
    U32              ubox_did,
    U32              control_msr,
    U32              ctl_val,
    DEVICE_CALLBACK  callback
);

extern VOID 
UNC_COMMON_PCI_Enable_PMU(
    PVOID            param, 
    U32              control_msr,
    U32              enable_val,
    U32              disable_val,
    DEVICE_CALLBACK  callback
);


extern VOID
UNC_COMMON_PCI_Disable_PMU(
    PVOID            param,
    U32              control_msr,
    U32              enable_val,
    U32              disable_val,
    DEVICE_CALLBACK  callback
);

extern VOID
UNC_COMMON_PCI_Clean_Up(
    PVOID param
);

extern VOID
UNC_COMMON_PCI_Read_Counts(
    PVOID  param, 
    U32    id
); 

extern VOID 
UNC_COMMON_PCI_Read_PMU_Data(
    PVOID   param
);

extern VOID 
UNC_COMMON_PCI_Scan_For_Uncore(
    PVOID           param,
    U32             dev_info_node,
    DEVICE_CALLBACK callback
);

/************************************************************/
/*
 * UNC common MSR  based API
 *
 ************************************************************/

extern VOID
UNC_COMMON_MSR_Write_PMU (
    PVOID            param,
    U32              control_msr,
    U64              control_val,
    U64              reset_val,
    DEVICE_CALLBACK  callback
);

extern VOID 
UNC_COMMON_MSR_Enable_PMU(
    PVOID            param, 
    U32              control_msr,
    U64              control_val,
    U64              unit_ctl_val,
    U64              pmon_ctl_val,
    DEVICE_CALLBACK  callback
);


extern VOID 
UNC_COMMON_MSR_Disable_PMU(
    PVOID            param, 
    U32              control_msr,
    U64              unit_ctl_val,
    U64              pmon_ctl_val,
    DEVICE_CALLBACK  callback
);

extern VOID
UNC_COMMON_MSR_Read_Counts(
    PVOID  param, 
    U32    id
); 

extern VOID
UNC_COMMON_MSR_Read_Counts_With_Mask(
    PVOID  param, 
    U32    id,
    U64    mask
); 

extern VOID 
UNC_COMMON_MSR_Read_PMU_Data(
    PVOID   param
);

extern VOID 
UNC_COMMON_MSR_Clean_Up(
    PVOID   param
);


#endif
