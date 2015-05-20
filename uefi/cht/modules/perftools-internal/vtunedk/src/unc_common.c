/*COPYRIGHT**
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.

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


#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "lwpmudrv.h"
#include "utility.h"
#include "control.h"
#include "output.h"
#include "unc_common.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "inc/pci.h"

extern UNCORE_TOPOLOGY_INFO_NODE uncore_topology;
extern U64                      *read_counter_info;


U32                             *unc_package_to_bus_map;


/************************************************************/
/* 
 * unc common Dummy Dispatch function
 *
 ************************************************************/
extern  void
UNC_COMMON_Dummy_Func (
    PVOID  param
)
{
    return;
}

/************************************************************/
/* 
 * UNC common PCI  based API
 *
 ************************************************************/


/*!
 * @fn          VOID UNC_COMMON_Do_Bus_to_Socket_Map(VOID)
 * 
 * @brief       This code discovers which package's data is read off of which bus.
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 *     This probably will move to the UBOX once that is programmed.
 */
VOID
UNC_COMMON_Do_Bus_to_Socket_Map(
    U32 socketid_ubox_did
)
{
    U32 bus_no, device_no, function_no;
    U32 pci_address;
    U32 value;
    U32 vendor_id;
    U32 device_id;
    U32 gid;
    U32 mapping;
    U32 i;

    if (unc_package_to_bus_map != NULL) {
        return;
    }

    unc_package_to_bus_map = CONTROL_Allocate_Memory(num_packages * sizeof(U32));

    if (unc_package_to_bus_map == NULL) {
        SEP_PRINT_DEBUG("UNC_COMMON_Do_Bus_to_Socket_Map allocated NULL by CONTROL_Allocate_Memory\n");
        return;
    }

    for (bus_no = 0; bus_no < MAX_PCI_BUSNO; bus_no++) {
        for (device_no = 0; device_no < MAX_PCI_DEVNO; device_no++) {
            for (function_no = 0; function_no < MAX_PCI_FUNCNO; function_no++) {

                // find the bus, device, and function number for
                // the socket ID UBOX device

                pci_address = FORM_PCI_ADDR(bus_no,
                                            device_no,
                                            function_no,
                                            0);
                value = PCI_Read_Ulong(pci_address);

                vendor_id = value & VENDOR_ID_MASK;
                device_id = (value & DEVICE_ID_MASK) >> DEVICE_ID_BITSHIFT;

                if (vendor_id != DRV_IS_PCI_VENDOR_ID_INTEL) {
                    continue;
                }
                if (device_id == socketid_ubox_did) {
                    // first get node id for the local socket
                    pci_address = FORM_PCI_ADDR(bus_no,
                                                device_no,
                                                function_no,
                                                UNCORE_SOCKETID_UBOX_LNID_OFFSET);
                    gid = PCI_Read_Ulong(pci_address) & 0x00000007;

                    // Get the node id mapping register:
                    // Basic idea is to read the Node ID Mapping Register (below)
                    // and match up one of the nodes with the gid that we read in above
                    // from the Node ID configuration register (above).
                    // Every three bits in the Node ID Mapping Register maps to a
                    // particular node (or package).  So, bits 2:0 maps to package 0,
                    // bits 5:3 maps to package 1, and so on.  Thus, we have to parse through
                    // every single triplet of bits to figure out the match.
                    pci_address = FORM_PCI_ADDR(bus_no,
                                                device_no,
                                                function_no,
                                                UNCORE_SOCKETID_UBOX_GID_OFFSET);
                    mapping = PCI_Read_Ulong(pci_address);

                    for (i = 0; i < 7; i++, mapping = mapping >> 3) {
                        if ((mapping & 0x00000007) == gid) {
                          unc_package_to_bus_map[i] = bus_no;
                          break;
                        }
                    }
                }
            }
        }
    }
}


/*!
 * @fn          extern VOID UNC_COMMON_PCI_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_PCI_Write_PMU (
    PVOID            param,
    U32              ubox_did,
    U32              control_msr,
    U32              ctl_val,
    DEVICE_CALLBACK  callback
)
{
    U32                        pci_address;
    U32                        device_id;
    U32                        dev_idx       = *((U32*)param);
    U32                        value;
    U32                        vendor_id;
    U32                        busno;
    U32                        this_cpu      = CONTROL_THIS_CPU();
    CPU_STATE                  pcpu          = &pcb[this_cpu];
    U32                        package_num   = core_to_package_map[this_cpu];


    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    // first, figure out which package maps to which bus
    UNC_COMMON_Do_Bus_to_Socket_Map(ubox_did);

    busno = unc_package_to_bus_map[package_num];

    FOR_EACH_REG_ENTRY_UNC(pecb,dev_idx,idx) {
        if (control_msr  && (ECB_entries_reg_id(pecb,idx) == control_msr)) {
             //Check if we need to zero this MSR out
             SYS_Write_MSR(ECB_entries_reg_id(pecb,idx), 0LL);
             SEP_PRINT_DEBUG("UNC_COMMON_PCI_Write_PMU wrote GLOBAL_CONTROL_MSR 0x%x\n", control_msr);
             continue;
        }

        // otherwise, we have a valid entry
        // now we just need to find the corresponding bus #
        pci_address = FORM_PCI_ADDR(busno,
                                    ECB_entries_dev_no(pecb,idx),
                                    ECB_entries_func_no(pecb,idx),
                                    0);
        value = PCI_Read_Ulong(pci_address);

        CHECK_IF_GENUINE_INTEL_DEVICE(value, vendor_id, device_id);
           
        if (callback                              &&
            callback->is_Valid_For_Write          &&
            !(callback->is_Valid_For_Write(device_id, ECB_entries_reg_id(pecb,idx)))) {
            continue;
        }

        if (ctl_val                                  &&
            callback                                 &&
            callback->is_Unit_Ctl                    &&
            (ECB_entries_reg_type(pecb,idx) == CCCR) &&
             callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,idx))) {
             value = ctl_val;
             // busno can not be stored in ECB because different sockets have different bus no.
             pci_address = FORM_PCI_ADDR(busno,
                                         ECB_entries_dev_no(pecb,idx),
                                         ECB_entries_func_no(pecb,idx),
                                         ECB_entries_reg_id(pecb,idx));
             // reset the counters
             PCI_Write_Ulong(pci_address, value);
             SEP_PRINT_DEBUG("UNC_COMMON_PCI_Write_PMU cpu=%d, reg = 0x%x --- value 0x%x\n",
                             this_cpu, ECB_entries_reg_id(pecb,idx), value);
             continue;
        } 

        // now program at the corresponding offset
        pci_address = FORM_PCI_ADDR(busno,
                                    ECB_entries_dev_no(pecb,idx),
                                    ECB_entries_func_no(pecb,idx),
                                    ECB_entries_reg_id(pecb,idx));
        PCI_Write_Ulong(pci_address, (U32)ECB_entries_reg_value(pecb,idx));

        SEP_PRINT_DEBUG("UNC_COMMON_PCI_Write_PMU cpu=%d, reg = 0x%x --- value 0x%x\n",
                             this_cpu, ECB_entries_reg_id(pecb,idx), (U32)ECB_entries_reg_value(pecb,idx));

        // we're zeroing out a data register, which is 48 bits long
        // we need to zero out the upper bits as well
        if (ECB_entries_reg_type(pecb,idx) == DATA) {
            pci_address = FORM_PCI_ADDR(busno,
                                        ECB_entries_dev_no(pecb,idx),
                                        ECB_entries_func_no(pecb,idx),
                                        (ECB_entries_reg_id(pecb,idx) + NEXT_ADDR_OFFSET));
            PCI_Write_Ulong(pci_address, (U32)ECB_entries_reg_value(pecb,idx));

            SEP_PRINT_DEBUG("UNC_COMMON_PCI_Write_PMU cpu=%d, reg = 0x%x --- value 0x%x\n",
                             this_cpu, ECB_entries_reg_id(pecb,idx), (U32)ECB_entries_reg_value(pecb,idx));
        }   

            // this is needed for overflow detection of the accumulators.
        if (LWPMU_DEVICE_counter_mask(&devices[dev_idx]) == 0) {
             LWPMU_DEVICE_counter_mask(&devices[dev_idx]) = (U64)ECB_entries_max_bits(pecb,idx);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;

    return;
}

/*!
 * @fn         static VOID UNC_COMMON_PCI_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the EVSEL registers
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_PCI_Enable_PMU (
    PVOID               param,
    U32                 control_msr,
    U32                 enable_val,
    U32                 disable_val,
    DEVICE_CALLBACK     callback
)
{
    U32            dev_idx       = *((U32 *)param);
    U32            value         = 0;
    U32            pci_address   = 0;
    U32            busno;
    U32            package_num;
    U32            this_cpu      = CONTROL_THIS_CPU();
    CPU_STATE      pcpu          = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    package_num        = core_to_package_map[this_cpu];
    busno              = unc_package_to_bus_map[package_num];

    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        if (ECB_entries_reg_id(pecb,i) == control_msr) {
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));
            SEP_PRINT_DEBUG("UNC_COMMON_PCI_Write_PMU wrote GLOBAL_CONTROL_MSR 0x%x val=0x%x\n",
                         control_msr, ECB_entries_reg_value(pecb,i));
            continue;
        }
        if (callback                               &&
            callback->is_PMON_Ctl                  &&
            (ECB_entries_reg_type(pecb,i) == CCCR) &&
             callback->is_PMON_Ctl(ECB_entries_reg_id(pecb,i))) {
             value = enable_val | ECB_entries_reg_value(pecb,i);
             pci_address = FORM_PCI_ADDR(busno,
                                         ECB_entries_dev_no(pecb,i),
                                         ECB_entries_func_no(pecb,i),
                                         ECB_entries_reg_id(pecb,i));
             PCI_Write_Ulong(pci_address, value);
             SEP_PRINT_DEBUG("UNC_COMMON_PCI_Enable_PMU Event_reg = 0x%x --- value 0x%x\n",
                          ECB_entries_reg_id(pecb,i), value);
             continue;
        }
        if (disable_val                            &&
            callback                               &&
            callback->is_Unit_Ctl                  &&
            callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,i))) {
            pci_address = FORM_PCI_ADDR(busno,
                                        ECB_entries_dev_no(pecb,i),
                                        ECB_entries_func_no(pecb,i),
                                        ECB_entries_reg_id(pecb,i));
            value = PCI_Read_Ulong(pci_address);
            value &= ~(disable_val);
            PCI_Write_Ulong(pci_address, value);
            SEP_PRINT_DEBUG("UNC_COMMON_PCI_Enable_PMU Event_reg = 0x%x --- value 0x%x\n",
                         ECB_entries_reg_id(pecb,i), value);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;

    return;
}

/*!
 * @fn           extern VOID UNC_COMMON_PCI_Disable_PMU(PVOID)
 *
 * @brief        Disable the per unit global control to stop the PMU counters.
 *
 * @param        Device Index of this PMU unit
 * @control_msr  Control MSR address
 * @enable_val   If counter freeze bit does not work, counter enable bit should be cleared
 * @disable_val  Disable collection
 *
 * @return       None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_PCI_Disable_PMU (
    PVOID               param,
    U32                 control_msr,
    U32                 enable_val,
    U32                 disable_val,
    DEVICE_CALLBACK callback
)
{
    U32 dev_idx                  = *((U32 *)param);
    U32 value;
    U32 pci_address;
    U32 busno;
    U32 package_num;
    U32 this_cpu                 = CONTROL_THIS_CPU();
    CPU_STATE      pcpu          = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    package_num        = core_to_package_map[this_cpu];
    busno              = unc_package_to_bus_map[package_num];

    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        if (control_msr && (ECB_entries_reg_id(pecb,i) == control_msr)) {
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), 0LL);
            SEP_PRINT_DEBUG("UNC_COMMON_PCI_Disable_PMU wrote GLOBAL_CONTROL_MSR 0x%x\n", control_msr);
            continue;
        }

        if (callback) {
            // The enable bit must be cleared when the PMU freeze is not working
            if (enable_val && callback->is_PMON_Ctl    &&
                (ECB_entries_reg_type(pecb,i) == CCCR) &&
                callback->is_PMON_Ctl(ECB_entries_reg_id(pecb,i))) {
                value = (~enable_val) & ECB_entries_reg_value(pecb,i);
                pci_address = FORM_PCI_ADDR(busno,
                                        ECB_entries_dev_no(pecb,i),
                                        ECB_entries_func_no(pecb,i),
                                        ECB_entries_reg_id(pecb,i));
                PCI_Write_Ulong(pci_address, value);
                SEP_PRINT_DEBUG("UNC_COMMON_PCI_Disable_PMU cpu=%d, Event_reg = 0x%x --- value 0x%x\n",
                         this_cpu, ECB_entries_reg_id(pecb,i), value);
            }
            else if (callback->is_Unit_Ctl                   &&
                     (ECB_entries_reg_type(pecb,i) == CCCR)  &&
                     callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,i))) {
                value = disable_val | (U32)ECB_entries_reg_value(pecb,i);
                pci_address = FORM_PCI_ADDR(busno,
                                        ECB_entries_dev_no(pecb,i),
                                        ECB_entries_func_no(pecb,i),
                                        ECB_entries_reg_id(pecb,i));
                PCI_Write_Ulong(pci_address, value);
                SEP_PRINT_DEBUG("UNC_COMMON_PCI_Disable_PMU cpu=%d, Event_Data_reg = 0x%x --- value 0x%x\n",
                         this_cpu, ECB_entries_reg_id(pecb,i), value);
            }
        }
    } END_FOR_EACH_REG_ENTRY_UNC;
    return;
}
 

/*!
 * @fn         extern VOID UNC_COMMON_PCI_Clean_Up(PVOID)
 *
 * @brief      clear out out programming
 *
 * @param      None
 *
 * @return     None
 */
extern void
UNC_COMMON_PCI_Clean_Up (
    VOID   *param
)
{
    if (unc_package_to_bus_map) {
        unc_package_to_bus_map = CONTROL_Free_Memory(unc_package_to_bus_map);
    }

    return;
}

/*!
 * @fn       extern void UNC_COMMON_PCI_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 * @param    id       The event id for the which the sample is generated
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *           Uncore PMU does not support sampling, i.e. ignore the id parameter.
 */
extern  VOID
UNC_COMMON_PCI_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64            *data       = (U64*) param;
    U32             cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB             pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];
    U32             pci_address;
    U32             this_cpu            = CONTROL_THIS_CPU();
    U32             package_num         = core_to_package_map[this_cpu];
    U32             bus_no              = unc_package_to_bus_map[package_num];


    // Write GroupID
    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;

    //Read in the counts into temporary buffer
    FOR_EACH_DATA_REG_UNC(pecb, id, i) {
        data  = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        // read lower 4 bytes
        pci_address                 = FORM_PCI_ADDR(bus_no,
                                                    ECB_entries_dev_no(pecb,i),
                                                    ECB_entries_func_no(pecb,i),
                                                    ECB_entries_reg_id(pecb,i));
        *data = LOWER_4_BYTES_MASK & PCI_Read_Ulong(pci_address);

        // read upper 4 bytes
        pci_address                 = FORM_PCI_ADDR(bus_no,
                                                    ECB_entries_dev_no(pecb,i),
                                                    ECB_entries_func_no(pecb,i),
                                                    (ECB_entries_reg_id(pecb,i) + NEXT_ADDR_OFFSET));
        *data |= (U64)PCI_Read_Ulong(pci_address) << NEXT_ADDR_SHIFT;
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/*!
 * @fn       extern   UNC_COMMON_PCI_Read_PMU_Data(param)
 *
 * @param    param    The device index
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer;
 */
extern VOID
UNC_COMMON_PCI_Read_PMU_Data(
    PVOID           param
)
{
    U32             dev_idx             = *((U32*)param);
    U32             pci_address;
    U64             value_low           = 0;
    U64             value_high          = 0;
    U32             this_cpu            = CONTROL_THIS_CPU();
    U32             package_num         = 0;
    U32             bus_no              = 0;
    U64            *buffer              = read_counter_info;
    DRV_CONFIG      pcfg_unc;
    U64             start_index;
    CPU_STATE       pcpu                = &pcb[this_cpu];
    U64             j                   = 0;
    U32             sub_evt_index       = 0;
    U32             prev_ei             = -1;
    U32             cur_ei              = 0;
    U32             cur_grp             = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    ECB             pecb                = LWPMU_DEVICE_PMU_register_data(&devices[(dev_idx)])[cur_grp];
    U32             num_events          = 0;

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    if (pecb) {
        num_events = ECB_num_events(pecb);
    }

    package_num         = core_to_package_map[this_cpu];
    bus_no              = unc_package_to_bus_map[package_num];
    pcfg_unc            = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    start_index         = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);

    //Read in the counts into temporary buffer
    FOR_EACH_DATA_REG_UNC(pecb,dev_idx,i) {
        cur_ei = ECB_entries_group_index(pecb, i);
        //the buffer index for this PMU needs to account for each event
        j = start_index +  ECB_entries_group_index(pecb, i) +
            ECB_entries_emon_event_id_index_local(pecb,i) +
            sub_evt_index*num_packages*LWPMU_DEVICE_num_units(&devices[dev_idx])+
            package_num * LWPMU_DEVICE_num_units(&devices[dev_idx]);

        // read lower 4 bytes
        pci_address                 = FORM_PCI_ADDR(bus_no,
                                                    ECB_entries_dev_no(pecb,i),
                                                    ECB_entries_func_no(pecb,i),
                                                    ECB_entries_reg_id(pecb,i));
        
        value_low = LOWER_4_BYTES_MASK & PCI_Read_Ulong(pci_address);

        // read upper 4 bytes
        pci_address                 = FORM_PCI_ADDR(bus_no,
                                                    ECB_entries_dev_no(pecb,i),
                                                    ECB_entries_func_no(pecb,i),
                                                    (ECB_entries_reg_id(pecb,i) + NEXT_ADDR_OFFSET));
        value_high                  = (U64)PCI_Read_Ulong(pci_address);
        buffer[j] = (value_high << NEXT_ADDR_SHIFT) | value_low;
        SEP_PRINT_DEBUG("j = %d value = %llu pkg = %d  e_id = %d\n",j, buffer[j],package_num, ECB_entries_emon_event_id_index_local(pecb,i));
        //Increment sub_evt_index so that the next event position is adjusted
        if ((prev_ei == -1 )|| (prev_ei != cur_ei)) {
             prev_ei = cur_ei;
             sub_evt_index++;
        }
        if (sub_evt_index == num_events) {
            sub_evt_index = 0;
        }
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}


/*!
 * @fn          static VOID UNC_COMMON_PCI_Scan_For_Uncore(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_PCI_Scan_For_Uncore(
    PVOID           param,
    U32             dev_node,
    DEVICE_CALLBACK callback
)
{
    U32                        pci_address;
    U32                        device_id;
    U32                        value;
    U32                        vendor_id;
    U32                        busno;
    U32                        j, k;

    for (busno = 0; busno < 256; busno++) {
         for (j=0; j< MAX_PCI_DEVNO;j++) {
             if (!(UNCORE_TOPOLOGY_INFO_pcidev_valid(&uncore_topology, dev_node, j))) {
                 continue;
             }
             for(k=0;k<MAX_PCI_FUNCNO;k++) {
                 if (!(UNCORE_TOPOLOGY_INFO_pcidev_is_devno_funcno_valid(&uncore_topology,dev_node,j,k))) {
                     continue;
                 }
                 pci_address = FORM_PCI_ADDR(busno,
                                             j,
                                             k,
                                             0);
                 value = PCI_Read_Ulong(pci_address);

                 CHECK_IF_GENUINE_INTEL_DEVICE(value, vendor_id, device_id);

                 SEP_PRINT_DEBUG("iMC device ID = 0x%d\n",device_id);
                 if ( callback && callback->is_Valid_Device && !callback->is_Valid_Device(device_id)) {
                     continue;
                 }
                 UNCORE_TOPOLOGY_INFO_pcidev_is_found_in_platform(&uncore_topology, dev_node, j, k) = 1;
                 SEP_PRINT_DEBUG("found device %d at B:D:F = %d:%d:%d\n", dev_node, busno,j,k);
             }
         }
    }

    return;
}


/************************************************************/
/*
 * UNC common MSR  based API
 *
 ************************************************************/


/*!
 * @fn          extern VOID UNC_COMMON_MSR_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_MSR_Write_PMU (
    PVOID            param,
    U32              control_msr,
    U64              control_val,
    U64              unit_reset_val,
    DEVICE_CALLBACK  callback
)
{
    U32            dev_idx       = *((U32*)param);
    U64            value         = 0;
    U32            this_cpu      = CONTROL_THIS_CPU();
    CPU_STATE      pcpu          = &pcb[this_cpu];
 
    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    if (control_msr) {
        SYS_Write_MSR(control_msr, control_val);
    }
    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        /*
        * Writing the GLOBAL Control register enables the PMU to start counting.
        * So write 0 into the register to prevent any counting from starting.
        */
        if (ECB_entries_reg_id(pecb,i) == control_msr) {
            continue;
        }
        if (unit_reset_val                         &&
            callback                               &&
            callback->is_Unit_Ctl                  &&
            callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,i))) {

            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), unit_reset_val);
            SEP_PRINT_DEBUG("common_sbox_Write_PMU Read reg = 0x%x --- value 0x%x\n",
                                     ECB_entries_reg_id(pecb,i), value);
            value = 0x0;
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
            SEP_PRINT_DEBUG("common_sbox_Write_PMU reg = 0x%x --- value 0x%x\n",
                                     ECB_entries_reg_id(pecb,i), value);
            continue;
        }

        SYS_Write_MSR(ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));
        SEP_PRINT_DEBUG("UNC_COMMON_MSR_Write_PMU Event_Data_reg = 0x%x --- value 0x%llx\n",
                        ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));

        // this is needed for overflow detection of the accumulators.
        if (LWPMU_DEVICE_counter_mask(&devices[dev_idx]) == 0) {
            LWPMU_DEVICE_counter_mask(&devices[dev_idx]) = (U64)ECB_entries_max_bits(pecb,i);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;

    return;
}


/*!
 * @fn         VOID UNC_COMMON_MSR_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the evsel registers
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
VOID
UNC_COMMON_MSR_Enable_PMU (
    PVOID               param,
    U32                 control_msr,
    U64                 control_value,
    U64                 unit_ctl_value,
    U64                 pmon_ctl_value,
    DEVICE_CALLBACK     callback
)
{
    U32            dev_idx       = *((U32*)param);
    U64            value         = 0;
    U32            this_cpu      = CONTROL_THIS_CPU();
    CPU_STATE      pcpu          = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        if (control_msr && (ECB_entries_reg_id(pecb,i) == control_msr)) {
            value = (control_value | ECB_entries_reg_value(pecb,i));
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
            SEP_PRINT_DEBUG("UNC_COMMON_MSR_Write_PMU wrote 0x%x\n", control_msr);
            continue;
        }
        if (callback                               &&
            callback->is_PMON_Ctl                  &&
            callback->is_PMON_Ctl(ECB_entries_reg_id(pecb,i))) {
            value = (pmon_ctl_value | ECB_entries_reg_value(pecb,i));
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
            SEP_PRINT_DEBUG("UNC_COMMON_MSR_Enable_PMU Event_Data_reg = 0x%x --- value 0x%I64x\n",
                         ECB_entries_reg_id(pecb,i), value);
            continue;
        }
        if (unit_ctl_value                         &&
            callback                               &&
            callback->is_Unit_Ctl                  &&
            callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,i))) {
            value = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            value &= ~(unit_ctl_value);
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;
    return;
}


/*!
 * @fn         extern VOID UNC_COMMON_MSR_Disable_PMU(PVOID)
 *
 * @brief      Disable the per unit global control to stop the PMU counters.
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
extern VOID
UNC_COMMON_MSR_Disable_PMU (
    PVOID               param,
    U32                 control_msr,
    U64                 unit_ctl_value,
    U64                 pmon_ctl_value,
    DEVICE_CALLBACK     callback
)
{
    U32            dev_idx       = *((U32*)param);
    U64            value         = 0;
    U32            this_cpu      = CONTROL_THIS_CPU();
    CPU_STATE      pcpu          = &pcb[this_cpu];

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }

    if (control_msr) {
        SYS_Write_MSR(control_msr, 0LL);
    }
    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        if (ECB_entries_reg_id(pecb,i) == control_msr) {
            continue;
        }
        if (callback                                &&
            callback->is_Unit_Ctl                   &&
            callback->is_Unit_Ctl(ECB_entries_reg_id(pecb,i))) {
            value = unit_ctl_value | ECB_entries_reg_value(pecb,i);
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
            SEP_PRINT_DEBUG("UNC_COMMON_MSR_Disable_PMU Event_Data_reg = 0x%x --- value 0x%I64x\n",
                         ECB_entries_reg_id(pecb,i), value);
            continue;
        }
        if (pmon_ctl_value                          &&
            callback                                &&
            callback->is_PMON_Ctl                   &&
            callback->is_PMON_Ctl(ECB_entries_reg_id(pecb,i))) {
            value = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            value &= ~(pmon_ctl_value);
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), value);
            SEP_PRINT_DEBUG("UNC_COMMON_MSR_Disable_PMU Event_Data_reg = 0x%x --- value 0x%I64x\n",
                         ECB_entries_reg_id(pecb,i), value);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;
    return;
}


/*!
 * @fn UNC_COMMON_MSR_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 * @param    id       The event id for the which the sample is generated
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 */
VOID
UNC_COMMON_MSR_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64  *data       = (U64*) param;
    U32   cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB   pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];

    // Write GroupID
    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;

    FOR_EACH_DATA_REG_UNC(pecb, id, i) {
        data  = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        *data = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}

/*!
 * @fn UNC_COMMON_MSR_Read_Counts_With_Mask(param, id, mask)
 *
 * @param    param    The read thread node to process
 * @param    id       The event id for the which the sample is generated
 * @param    mask     The mask bits for value
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 */
VOID
UNC_COMMON_MSR_Read_Counts_With_Mask (
    PVOID  param,
    U32    id,
    U64    mask
)
{
    U64  *data       = (U64*) param;
    U32   cur_grp    = LWPMU_DEVICE_cur_group(&devices[id]);
    ECB   pecb       = LWPMU_DEVICE_PMU_register_data(&devices[id])[cur_grp];

    if (!mask) {
        return UNC_COMMON_MSR_Read_Counts(param, id);
    }

    // Write GroupID
    data    = (U64*)((S8*)data + ECB_group_offset(pecb));
    *data   = cur_grp + 1;

    FOR_EACH_DATA_REG_UNC(pecb, id, i) {
        data  = (U64 *)((S8*)param + ECB_entries_counter_event_offset(pecb,i));
        *data = SYS_Read_MSR(ECB_entries_reg_id(pecb,i)) & mask;
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}


/*!
 * @fn UNC_COMMON_MSR_Read_PMU_Data(param)
 *
 * @param    param    The read thread node to process
 * @param    id       The id refers to the device index
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer
 *           Let us say we have 2 core events in a dual socket JKTN;
 *           The start_index will be at 32 as it will 2 events in 16 CPU per socket
 *           The position for first event of QPI will be computed based on its event
 *
 */
VOID
UNC_COMMON_MSR_Read_PMU_Data (
    PVOID  param
)
{
    U32             dev_idx             = *((U32*)param);
    U32             this_cpu            = CONTROL_THIS_CPU();
    U32             package_num         = 0;
    U64            *buffer              = read_counter_info;
    DRV_CONFIG      pcfg_unc;
    U64             start_index;
    CPU_STATE       pcpu                = &pcb[this_cpu];
    U64             j                   = 0;
    U32             sub_evt_index       = 0;
    U32             prev_ei             = -1;
    U32             cur_ei              = 0;
    U32             cur_grp             = LWPMU_DEVICE_cur_group(&devices[(dev_idx)]);
    ECB             pecb                = LWPMU_DEVICE_PMU_register_data(&devices[(dev_idx)])[cur_grp];
    U32             num_events          = 0;

    if (!CPU_STATE_socket_master(pcpu)) {
        return;
    }
    if (pecb) {
        num_events = ECB_num_events(pecb);
    }
    package_num         = core_to_package_map[this_cpu];
    pcfg_unc            = (DRV_CONFIG)LWPMU_DEVICE_pcfg(&devices[dev_idx]);
    start_index         = DRV_CONFIG_emon_unc_offset(pcfg_unc, cur_grp);
    SEP_PRINT_DEBUG("offset for uncore group %d is %d num_pkgs = 0x%llx num_events = %d\n", cur_grp, start_index, num_packages, num_events);
    //Read in the counts into temporary buffer
    FOR_EACH_DATA_REG_UNC(pecb,dev_idx,i) {
            cur_ei = ECB_entries_group_index(pecb, i);
            //the buffer index for this PMU needs to account for each event
            j = start_index +  ECB_entries_group_index(pecb, i) +
                ECB_entries_emon_event_id_index_local(pecb,i) +
                sub_evt_index*num_packages*LWPMU_DEVICE_num_units(&devices[dev_idx])+
                package_num * LWPMU_DEVICE_num_units(&devices[dev_idx]);
                SEP_PRINT_DEBUG("%d + %d + %d + %d*%d*%d + %d * %d = j \n",
                      start_index,ECB_entries_group_index(pecb, i),ECB_entries_emon_event_id_index_local(pecb,i),
                      sub_evt_index,num_packages,LWPMU_DEVICE_num_units(&devices[dev_idx]), package_num,LWPMU_DEVICE_num_units(&devices[dev_idx]));
            buffer[j] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            SEP_PRINT_DEBUG("j = %d value = 0x%x pkg = %d  e_id = %d\n",j, buffer[j], package_num, ECB_entries_emon_event_id_index_local(pecb,i));
            //Increment sub_evt_index so that the next event position is adjusted
            if ((prev_ei == -1 )|| (prev_ei != cur_ei)) {
                 prev_ei = cur_ei;
                 sub_evt_index++;
            }
            if (sub_evt_index == num_events) {
                sub_evt_index = 0;
            }
    } END_FOR_EACH_DATA_REG_UNC;

    return;
}


/*!
 * @fn         VOID UNC_COMMON_MSR_Clean_Up(PVOID)
 *
 * @brief      clear out out programming
 *
 * @param      None
 *
 * @return     None
 */
VOID
UNC_COMMON_MSR_Clean_Up (
    VOID   *param
)
{
    U32 dev_idx = *((U32*)param);
 
    FOR_EACH_REG_ENTRY_UNC(pecb, dev_idx, i) {
        if (ECB_entries_clean_up_get(pecb,i)) {
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), 0LL);
        }
    } END_FOR_EACH_REG_ENTRY_UNC;

    return;
}

