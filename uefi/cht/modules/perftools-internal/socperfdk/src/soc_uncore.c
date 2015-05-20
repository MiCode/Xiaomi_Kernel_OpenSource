/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2013-2014 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  BSD LICENSE 

  Copyright(c) 2013-2014 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ***********************************************************************************************
*/


#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "socperfdrv.h"
#include "control.h"
#include "soc_uncore.h"
#include "inc/ecb_iterators.h"
#include "inc/pci.h"

#if defined (PCI_HELPERS_API)
#include <asm/intel_mid_pcihelpers.h>
#elif defined(DRV_CHROMEOS)
#include <linux/pci.h>
static struct pci_dev *pci_root = NULL;
#define PCI_DEVFN(slot, func)   ((((slot) & 0x1f) << 3) | ((func) & 0x07))
#endif

static U32            counter_overflow[UNCORE_MAX_COUNTERS];
static U32            counter_port_id         = 0;
extern U64           *read_unc_ctr_info;
//global variable for reading  counter values
static U64           *uncore_current_data    = NULL;
static U64           *uncore_to_read_data    = NULL;
static U32            device_id            = 0;
static U64            trace_virtual_address = 0;


#if defined(DRV_CHROMEOS)
/*!
 * @fn          static VOID get_pci_device_handle(U32   bus_no,
                                                  U32   dev_no,
                                                  U32   func_no)
 *
 * @brief       Get PCI device handle to be able to read/write
 *
 * @param       bus_no      - bus number
 *              dev_no      - device number
 *              func_no     - function number
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static void
get_pci_device_handle (
    U32   bus_no,
    U32   dev_no,
    U32   func_no
)
{
    if (!pci_root) {
        pci_root = pci_get_bus_and_slot(bus_no, PCI_DEVFN(dev_no, func_no));
        if (!pci_root) {
            SOCPERF_PRINT_DEBUG("Unable to get pci device handle");
        }
    }

    return;
}
#endif

/*!
 * @fn          static VOID write_To_Register(U32   bus_no,
                                              U32   dev_no,
                                              U32   func_no,
                                              U32   port_id,
                                              U32   op_code,
                                              U64   mmio_offset,
                                              ULONG value)
 *
 * @brief       Reads Uncore programming
 *
 * @param       bus_no      - bus number
 *              dev_no      - device number
 *              func_no     - function number
 *              port_id     - port id
 *              op_code     - operation code
 *              mmio_offset - mmio offset
 *              value       - data to be written to the register
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static void
write_To_Register (
    U32   bus_no,
    U32   dev_no,
    U32   func_no,
    U32   port_id,
    U32   op_code,
    U64   mmio_offset,
    ULONG value
)
{
    U32 cmd  = 0;
    U32 mmio_offset_lo;
    U32 mmio_offset_hi;
#if !defined(DRV_CHROMEOS) && !defined(PCI_HELPERS_API)
    U32 pci_address;
#endif

    mmio_offset_hi = mmio_offset & SOC_UNCORE_OFFSET_HI_MASK;
    mmio_offset_lo = mmio_offset & SOC_UNCORE_OFFSET_LO_MASK;
    cmd            = (op_code << SOC_UNCORE_OP_CODE_SHIFT) +
                       (port_id << SOC_UNCORE_PORT_ID_SHIFT) +
                       (mmio_offset_lo << 8) +
                       (SOC_UNCORE_BYTE_ENABLES << 4);
    SOCPERF_PRINT_DEBUG("write off=%llx value=%x\n", mmio_offset, value);

#if defined (PCI_HELPERS_API)
    intel_mid_msgbus_write32_raw_ext(cmd, mmio_offset_hi, value);
#elif defined(DRV_CHROMEOS)
    if (!pci_root) {
        get_pci_device_handle(bus_no, dev_no, func_no);
    }
    pci_write_config_dword(pci_root, SOC_UNCORE_MDR_REG_OFFSET, value);
    pci_write_config_dword(pci_root, SOC_UNCORE_MCRX_REG_OFFSET, mmio_offset_hi);
    pci_write_config_dword(pci_root, SOC_UNCORE_MCR_REG_OFFSET, cmd);
#else
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MDR_REG_OFFSET);
    PCI_Write_Ulong((ULONG)pci_address, (ULONG)value);
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MCRX_REG_OFFSET);
    PCI_Write_Ulong((ULONG)pci_address, mmio_offset_hi);
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MCR_REG_OFFSET);
    PCI_Write_Ulong((ULONG)pci_address, cmd);
#endif

    return;
}

/*!
 * @fn          static ULONG read_From_Register(U32 bus_no,
                                                U32 dev_no,
                                                U32 func_no,
                                                U32 port_id,
                                                U32 op_code,
                                                U64 mmio_offset)
 *
 * @brief       Reads Uncore programming info
 *
 * @param       bus_no      - bus number
 *              dev_no      - device number
 *              func_no     - function number
 *              port_id     - port id
 *              op_code     - operation code
 *              mmio_offset - mmio offset
 *
 * @return      data from the counter
 *
 * <I>Special Notes:</I>
 */
static void
read_From_Register (
    U32  bus_no,
    U32  dev_no,
    U32  func_no,
    U32  port_id,
    U32  op_code,
    U64  mmio_offset,
    U32 *data_val
)
{
    U32 data = 0;
    U32 cmd  = 0;
    U32 mmio_offset_hi;
    U32 mmio_offset_lo;
#if !defined(DRV_CHROMEOS) && !defined(PCI_HELPERS_API)
    U32 pci_address;
#endif

    mmio_offset_hi = mmio_offset & SOC_UNCORE_OFFSET_HI_MASK;
    mmio_offset_lo = mmio_offset & SOC_UNCORE_OFFSET_LO_MASK;
    cmd      = (op_code << SOC_UNCORE_OP_CODE_SHIFT) +
                (port_id << SOC_UNCORE_PORT_ID_SHIFT) +
                (mmio_offset_lo << 8) +
                (SOC_UNCORE_BYTE_ENABLES << 4);

#if defined (PCI_HELPERS_API)
    data = intel_mid_msgbus_read32_raw_ext(cmd, mmio_offset_hi);
#elif defined(DRV_CHROMEOS)
    if (!pci_root) {
        get_pci_device_handle(bus_no, dev_no, func_no);
    }
    pci_write_config_dword(pci_root, SOC_UNCORE_MCRX_REG_OFFSET, mmio_offset_hi);
    pci_write_config_dword(pci_root, SOC_UNCORE_MCR_REG_OFFSET, cmd);
    pci_read_config_dword(pci_root, SOC_UNCORE_MDR_REG_OFFSET, &data);
#else
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MCRX_REG_OFFSET);
    PCI_Write_Ulong((ULONG)pci_address, mmio_offset_hi);
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MCR_REG_OFFSET);
    PCI_Write_Ulong((ULONG)pci_address, cmd);
    pci_address = FORM_PCI_ADDR(bus_no, dev_no, func_no, SOC_UNCORE_MDR_REG_OFFSET);
    data = PCI_Read_Ulong(pci_address);
#endif
    SOCPERF_PRINT_DEBUG("read off=%llx value=%x\n", mmio_offset, data);
    if (data_val) {
        *data_val = data;
    }

    return;
}

/*!
 * @fn          static VOID uncore_Reset_Counters(U32 dev_idx)
 *
 * @brief       Reset counters
 *
 * @param       dev_idx - device index
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Reset_Counters (
    U32 dev_idx
)
{
    U32 data_reg   = 0;

    if (counter_port_id != 0) {
        FOR_EACH_PCI_REG_RAW(pecb, i, dev_idx) {
            if (ECB_entries_reg_type(pecb,i) == CCCR) {
                data_reg           = i + ECB_cccr_pop(pecb);
                if (ECB_entries_reg_type(pecb,data_reg) == DATA) {
                    write_To_Register(ECB_entries_bus_no(pecb, data_reg),
                                      ECB_entries_dev_no(pecb, data_reg),
                                      ECB_entries_func_no(pecb, data_reg),
                                      counter_port_id,
                                      SOC_COUNTER_WRITE_OP_CODE,
                                      ECB_entries_pci_id_offset(pecb, data_reg),
                                      (ULONG)0);
                }
                write_To_Register(ECB_entries_bus_no(pecb, i),
                                  ECB_entries_dev_no(pecb, i),
                                  ECB_entries_func_no(pecb, i),
                                  counter_port_id,
                                  SOC_COUNTER_WRITE_OP_CODE,
                                  ECB_entries_pci_id_offset(pecb,i),
                                  (ULONG)SOC_UNCORE_STOP);
            }
        } END_FOR_EACH_PCI_REG_RAW;
    }

    return;
}

/*!
 * @fn          static VOID uncore_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the entries and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       param - device index
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Write_PMU (
    VOID  *param
)
{
    U32                        dev_idx;
    ECB                        pecb;
    DRV_PCI_DEVICE_ENTRY       dpden;
    U32                        pci_address;
    U32                        bar_lo;
    U64                        bar_hi;
    U64                        final_bar;
    U64                        physical_address;
    U32                        dev_index       = 0;
    S32                        bar_list[SOC_UNCORE_MAX_PCI_DEVICES];
    U32                        bar_index       = 0;
    U32                        map_size        = 0;
    U64                        virtual_address = 0;
    U32                        bar_name        = 0;
    DRV_PCI_DEVICE_ENTRY       curr_pci_entry  = NULL;
    U32                        next_bar_offset = 0;
    U64                        mmio_offset     = 0;
    U64                        map_base        = 0;
    U32                        i               = 0;
    U32                        cur_grp         = LWPMU_DEVICE_cur_group(device_uncore);

    dev_idx  = *((U32*)param);
    if (device_uncore == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: NULL device_uncore!\n");
        return;
    }
    pecb     = (ECB)LWPMU_DEVICE_PMU_register_data(device_uncore)[cur_grp];
    if (pecb == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: null pecb!\n");
        return;
    }

    device_id = dev_idx;
    for (dev_index = 0; dev_index < SOC_UNCORE_MAX_PCI_DEVICES; dev_index++) {
        bar_list[dev_index] = -1;
    }

    // initialize the per-counter overflow numbers
    for (i = 0; i < UNCORE_MAX_COUNTERS; i++) {
        counter_overflow[i]         = 0;
        pcb[0].last_uncore_count[i] = 0;
    }

    // Allocate memory for reading GMCH counter values + the group id
    if (!uncore_current_data) {
        uncore_current_data = CONTROL_Allocate_Memory((UNCORE_MAX_COUNTERS+1)*sizeof(U64));
        if (!uncore_current_data) {
            return;
        }
    }
    if (!uncore_to_read_data) {
        uncore_to_read_data = CONTROL_Allocate_Memory((UNCORE_MAX_COUNTERS+1)*sizeof(U64));
        if (!uncore_to_read_data) {
            return;
        }
    }

    ECB_pcidev_entry_list(pecb) = (DRV_PCI_DEVICE_ENTRY)((S8*)pecb + ECB_pcidev_list_offset(pecb));
    dpden = ECB_pcidev_entry_list(pecb);

    uncore_Reset_Counters(dev_idx);

    for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
        curr_pci_entry = &dpden[dev_index];
        bar_name       = DRV_PCI_DEVICE_ENTRY_bar_name(curr_pci_entry);
        mmio_offset    = DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry);

        if (counter_port_id == 0 && DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_COUNTER) {
            counter_port_id = DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry);
        }
        if (DRV_PCI_DEVICE_ENTRY_config_type(curr_pci_entry) == UNC_PCICFG) {
            if (bar_name == UNC_SOCPCI &&
                (DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_MUX ||
                DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_COUNTER) &&
                DRV_PCI_DEVICE_ENTRY_operation(curr_pci_entry) == UNC_OP_WRITE) {
                SOCPERF_PRINT_DEBUG("dev_index=%d OFFSET=%x VAL=%x\n", dev_index, DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry), DRV_PCI_DEVICE_ENTRY_value(curr_pci_entry));
                write_To_Register(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_op_code(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry),
                                  (ULONG)DRV_PCI_DEVICE_ENTRY_value(curr_pci_entry));
            }
            continue;
        }
        // UNC_MMIO programming
        if (bar_list[bar_name] != -1) {
            bar_index                                            = bar_list[bar_name];
            virtual_address                                      = DRV_PCI_DEVICE_ENTRY_virtual_address(&dpden[bar_index]);
            DRV_PCI_DEVICE_ENTRY_virtual_address(curr_pci_entry) = DRV_PCI_DEVICE_ENTRY_virtual_address(&dpden[bar_index]);
            writel(DRV_PCI_DEVICE_ENTRY_value(curr_pci_entry), (U32*)(((char*)(UIOP)virtual_address)+mmio_offset));
            continue;
        }
        pci_address     = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                    DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                    DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                    DRV_PCI_DEVICE_ENTRY_bar_offset(curr_pci_entry));
        bar_lo          = PCI_Read_Ulong(pci_address);
        next_bar_offset = DRV_PCI_DEVICE_ENTRY_bar_offset(curr_pci_entry)+SOC_UNCORE_NEXT_ADDR_OFFSET;
        pci_address     = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                    DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                    DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                    next_bar_offset);
        bar_hi      = PCI_Read_Ulong(pci_address);
        final_bar   = (bar_hi << SOC_UNCORE_BAR_ADDR_SHIFT) | bar_lo;
        final_bar  &= SOC_UNCORE_BAR_ADDR_MASK;
        DRV_PCI_DEVICE_ENTRY_bar_address(curr_pci_entry) = final_bar;
        physical_address = DRV_PCI_DEVICE_ENTRY_bar_address(curr_pci_entry);
        if (physical_address) {
            map_size = SOC_UNCORE_OTHER_BAR_MMIO_PAGE_SIZE;
            map_base = (mmio_offset/map_size)*map_size;
            if (mmio_offset > map_size) {
                physical_address = physical_address + map_base;
            }
        }
    }

    return;
}



/*!
 * @fn         static VOID uncore_Disable_PMU(PVOID)
 *
 * @brief      Unmap the virtual address when sampling/driver stops
 *
 * @param      param - device index
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Disable_PMU (
    PVOID  param
)
{
    U32 dev_idx   = *((U32*)param);

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP) {
        uncore_Reset_Counters(dev_idx);
    }
    uncore_current_data = CONTROL_Free_Memory(uncore_current_data);
    uncore_to_read_data = CONTROL_Free_Memory(uncore_to_read_data);

    return;
}


/*!
 * @fn         static VOID uncore_Stop_Mem(VOID)
 *
 * @brief      Stop trace
 *
 * @param      param - None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Stop_Mem (
    VOID
)
{
    ECB                   pecb;
    DRV_PCI_DEVICE_ENTRY  dpden;
    U32                   bar_name        = 0;
    DRV_PCI_DEVICE_ENTRY  curr_pci_entry  = NULL;
    U64                   mmio_offset     = 0;
    U32                   dev_index       = 0;
    U32                   data_val        = 0;
    U32                   cur_grp         = LWPMU_DEVICE_cur_group(device_uncore);

    if (device_uncore == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: NULL device_uncore!\n");
        return;
    }
    pecb     = (ECB)LWPMU_DEVICE_PMU_register_data(device_uncore)[cur_grp];
    if (pecb == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: null pecb!\n");
        return;
    }

    ECB_pcidev_entry_list(pecb) = (DRV_PCI_DEVICE_ENTRY)((S8*)pecb + ECB_pcidev_list_offset(pecb));
    dpden = ECB_pcidev_entry_list(pecb);

    for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
        curr_pci_entry = &dpden[dev_index];
        bar_name       = DRV_PCI_DEVICE_ENTRY_bar_name(curr_pci_entry);
        mmio_offset    = DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry);

        if (DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_STOP &&
            DRV_PCI_DEVICE_ENTRY_config_type(curr_pci_entry) == UNC_PCICFG &&
            bar_name == UNC_SOCPCI &&
            DRV_PCI_DEVICE_ENTRY_operation(curr_pci_entry) == UNC_OP_READ) {
                SOCPERF_PRINT_DEBUG("op=%d port=%d offset=%x val=%x\n",
                                DRV_PCI_DEVICE_ENTRY_op_code(curr_pci_entry),
                                DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                mmio_offset,
                                data_val);
                read_From_Register(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                   SOC_COUNTER_READ_OP_CODE,
                                   mmio_offset,
                                   &data_val);
                SOCPERF_PRINT_DEBUG("op=%d port=%d offset=%x val=%x\n",
                                DRV_PCI_DEVICE_ENTRY_op_code(curr_pci_entry),
                                DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                mmio_offset,
                                data_val);
                write_To_Register(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                  DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                  SOC_COUNTER_WRITE_OP_CODE,
                                  mmio_offset,
                                  (ULONG)(data_val | 0x2000));
        }
    }

    return;
}

/*!
 * @fn         static VOID uncore_Initialize(PVOID)
 *
 * @brief      Initialize any registers or addresses
 *
 * @param      param
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Initialize (
    VOID  *param
)
{
    return;
}


/*!
 * @fn         static VOID uncore_Clean_Up(PVOID)
 *
 * @brief      Reset any registers or addresses
 *
 * @param      param
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
uncore_Clean_Up (
    VOID   *param
)
{
    if (trace_virtual_address) {
        iounmap((void*)(UIOP)trace_virtual_address);
        trace_virtual_address = 0;
    }
    return;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn uncore_Read_Data()
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief    Read the counters
 *
 */
static VOID
uncore_Read_Data (
    PVOID data_buffer
)
{
    U32              event_id    = 0;
    U64             *data;
    int              data_index;
    U32              data_val    = 0;
    U32              data_reg    = 0;
    U64              total_count = 0;
    U32              event_index = 0;
    U32              cur_grp     = LWPMU_DEVICE_cur_group(device_uncore);

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_IDLE          ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RESERVED      ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP  ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
        SOCPERF_PRINT_ERROR("ERROR: RETURING EARLY from Read_Data\n");
        return;
    }

    data       = data_buffer;
    data_index = 0;

    preempt_disable();

    // Write GroupID
    data[data_index] = cur_grp + 1;
    // Increment the data index as the event id starts from zero
    data_index++;

    FOR_EACH_PCI_REG_RAW(pecb, i, dev_idx) {
        if (ECB_entries_reg_type(pecb,i) == CCCR) {
            write_To_Register(ECB_entries_bus_no(pecb, i),
                              ECB_entries_dev_no(pecb, i),
                              ECB_entries_func_no(pecb, i),
                              counter_port_id,
                              SOC_COUNTER_WRITE_OP_CODE,
                              ECB_entries_pci_id_offset(pecb,i),
                              (ULONG)SOC_UNCORE_SAMPLE_DATA);

            data_reg           = i + ECB_cccr_pop(pecb);
            if (ECB_entries_reg_type(pecb,data_reg) == DATA) {
                read_From_Register(ECB_entries_bus_no(pecb, data_reg),
                                   ECB_entries_dev_no(pecb, data_reg),
                                   ECB_entries_func_no(pecb, data_reg),
                                   counter_port_id,
                                   SOC_COUNTER_READ_OP_CODE,
                                   ECB_entries_pci_id_offset(pecb,data_reg),
                                   &data_val);
                if (data_val < pcb[0].last_uncore_count[event_index]) {
                    counter_overflow[event_index]++;
                }
                pcb[0].last_uncore_count[event_index] = data_val;
                total_count = data_val + counter_overflow[event_index]*UNCORE_MAX_COUNT;
                event_index++;
                data[data_index+event_id] = total_count;
                event_id++;
            }
        }

    } END_FOR_EACH_PCI_REG_RAW;

    preempt_enable();

    return;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn uncore_Create_Mem()
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief    Read the counters
 *
 */
static VOID
uncore_Create_Mem (
    U32  memory_size,
    U64 *trace_buffer
)
{
    ECB                        pecb;
    DRV_PCI_DEVICE_ENTRY       dpden;
    U32                        bar_name        = 0;
    DRV_PCI_DEVICE_ENTRY       curr_pci_entry  = NULL;
    U64                        mmio_offset     = 0;
    U32                        dev_index       = 0;
    U32                        data_val        = 0;
    U32                        reg_index       = 0;
    U64                        physical_high   = 0;
    U64                        odla_physical_address = 0;

    if (device_uncore == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: NULL device_uncore!\n");
        return;
    }
    pecb     = (ECB)LWPMU_DEVICE_PMU_register_data(device_uncore)[0];
    if (pecb == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: null pecb!\n");
        return;
    }

    if (!trace_buffer) {
        return;
    }

    ECB_pcidev_entry_list(pecb) = (DRV_PCI_DEVICE_ENTRY)((S8*)pecb + ECB_pcidev_list_offset(pecb));
    dpden = ECB_pcidev_entry_list(pecb);

    for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
        curr_pci_entry = &dpden[dev_index];
        bar_name       = DRV_PCI_DEVICE_ENTRY_bar_name(curr_pci_entry);
        mmio_offset    = DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry);

        if (DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_MEMORY &&
            DRV_PCI_DEVICE_ENTRY_config_type(curr_pci_entry) == UNC_PCICFG &&
            bar_name == UNC_SOCPCI &&
            DRV_PCI_DEVICE_ENTRY_operation(curr_pci_entry) == UNC_OP_WRITE) {
                read_From_Register(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                   DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                   SOC_COUNTER_READ_OP_CODE,
                                   mmio_offset,
                                   &data_val);
                if (reg_index == 1) {
                    odla_physical_address = data_val;
                }
                else if (reg_index == 2) {
                    physical_high = data_val;
                    odla_physical_address = odla_physical_address | (physical_high << 32);
                }
                SOCPERF_PRINT_DEBUG("op=%d port=%d offset=%x val=%x\n",
                                DRV_PCI_DEVICE_ENTRY_op_code(curr_pci_entry),
                                DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                                mmio_offset,
                                data_val);
                reg_index++;
        }
        continue;
    }
    SOCPERF_PRINT_DEBUG("Physical Address=%llx\n", odla_physical_address);
    if (odla_physical_address) {
        trace_virtual_address = (U64) (UIOP) ioremap_nocache(odla_physical_address, 1024*sizeof(U64));
        SOCPERF_PRINT_DEBUG("PHY=%llx ODLA VIRTUAL ADDRESS=%llx\n", odla_physical_address, trace_virtual_address);
        if (trace_buffer) {
           *trace_buffer = odla_physical_address;
        }
    }

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn uncore_Check_Status()
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief    Read the counters
 *
 */
static VOID
uncore_Check_Status (
    U64 *trace_buffer,
    U32 *num_entries
)
{
    U32                        dev_index       = 0;
    ECB                        pecb;
    DRV_PCI_DEVICE_ENTRY       dpden;
    U32                        bar_name        = 0;
    DRV_PCI_DEVICE_ENTRY       curr_pci_entry  = NULL;
    U64                        mmio_offset     = 0;
    U32                        data_val        = 0;
    U32                        data_index      = 0;

    if (device_uncore == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: NULL device_uncore!\n");
        return;
    }
    pecb  = (ECB)LWPMU_DEVICE_PMU_register_data(device_uncore)[0];
    if (pecb == NULL) {
        SOCPERF_PRINT_ERROR("ERROR: null pecb!\n");
        return;
    }
    if (!trace_buffer) {
        return;
    }

    ECB_pcidev_entry_list(pecb) = (DRV_PCI_DEVICE_ENTRY)((S8*)pecb + ECB_pcidev_list_offset(pecb));
    dpden = ECB_pcidev_entry_list(pecb);

    for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
        curr_pci_entry = &dpden[dev_index];
        bar_name       = DRV_PCI_DEVICE_ENTRY_bar_name(curr_pci_entry);
        mmio_offset    = DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry);

        if (DRV_PCI_DEVICE_ENTRY_prog_type(curr_pci_entry) == UNC_STATUS &&
            DRV_PCI_DEVICE_ENTRY_config_type(curr_pci_entry) == UNC_PCICFG &&
            bar_name == UNC_SOCPCI &&
            DRV_PCI_DEVICE_ENTRY_operation(curr_pci_entry) == UNC_OP_READ) {
            read_From_Register(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                               DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                               DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                               DRV_PCI_DEVICE_ENTRY_port_id(curr_pci_entry),
                               SOC_COUNTER_READ_OP_CODE,
                               mmio_offset,
                               &data_val);
            SOCPERF_PRINT_DEBUG("TRACE STATUS=%x\n", data_val);
            trace_buffer[data_index]  = data_val;
            data_index++;
            continue;
        }
    }

    if (num_entries) {
        *num_entries = data_index;
    }

    return;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn uncore_Read_Mem()
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief    Read the counters
 *
 */
static VOID
uncore_Read_Mem (
    U64  start_address,
    U64 *trace_buffer,
    U32  num_entries
)
{
    U32 data_index = 0;
    U32 data_value = 0;

    if (num_entries == 0 || !trace_buffer) {
        return;
    }
    SOCPERF_PRINT_DEBUG("Reading memory for num_entries=%d from address=%llx\n", num_entries, trace_virtual_address);
    for (data_index = 0; data_index < num_entries; data_index++) {
        if (trace_virtual_address) {
            data_value = readl((U64*)(UIOP)trace_virtual_address + data_index);

            SOCPERF_PRINT_DEBUG("DATA VALUE=%llx\n", data_value);
            *(trace_buffer + data_index) = data_value;
        }
    }

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  soc_uncore_dispatch =
{
    uncore_Initialize,                 // initialize
    NULL,                              // destroy
    uncore_Write_PMU,                  // write
    uncore_Disable_PMU,                // freeze
    NULL,                              // restart
    NULL,                              // read
    NULL,                              // check for overflow
    NULL,
    NULL,
    uncore_Clean_Up,
    NULL,
    NULL,
    NULL,
    NULL,                              // read counts
    NULL,
    NULL,
    NULL,
    NULL,
    uncore_Read_Data,
    uncore_Create_Mem,
    uncore_Check_Status,
    uncore_Read_Mem,
    uncore_Stop_Mem
};
