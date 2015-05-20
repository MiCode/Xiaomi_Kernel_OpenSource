/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011-2014 Intel Corporation. All rights reserved.

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

  Copyright(c) 2011-2014 Intel Corporation. All rights reserved.
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
#include <linux/wait.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "socperfdrv.h"
#include "control.h"
#include "haswellunc_sa.h"
#include "ecb_iterators.h"
#include "inc/pci.h"

static U64            counter_virtual_address = 0;
static U32            counter_overflow[HSWUNC_SA_MAX_COUNTERS];
extern LWPMU_DEVICE   device_uncore;
static U32            device_id = 0;

/*!
 * @fn          static VOID hswunc_sa_Write_PMU(VOID*)
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
hswunc_sa_Write_PMU (
    VOID  *param
)
{
    U32                        dev_idx  = *((U32*)param);
    U32                        cur_grp  = LWPMU_DEVICE_cur_group(device_uncore);
    ECB                        pecb     = LWPMU_DEVICE_PMU_register_data(device_uncore)[cur_grp];
    DRV_PCI_DEVICE_ENTRY       dpden;
    U32                        pci_address;
    U32                        bar_lo;
    U64                        bar_hi;
    U64                        final_bar;
    U64                        physical_address;
    U32                        dev_index       = 0;
    S32                        bar_list[HSWUNC_SA_MAX_PCI_DEVICES];
    U32                        bar_index       = 0;
    U64                        gdxc_bar        = 0;
    U32                        map_size        = 0;
    U64                        virtual_address = 0;
    U64                        mmio_offset     = 0;
    U32                        bar_name        = 0;
    DRV_PCI_DEVICE_ENTRY       curr_pci_entry  = NULL;
    U32                        next_bar_offset = 0;
    U32                        i               = 0;

    for (dev_index = 0; dev_index < HSWUNC_SA_MAX_PCI_DEVICES; dev_index++) {
        bar_list[dev_index] = -1;
    }

    device_id = dev_idx;
    // initialize the CHAP per-counter overflow numbers
    for (i = 0; i < HSWUNC_SA_MAX_COUNTERS; i++) {
        counter_overflow[i]          = 0;
        pcb[0].last_uncore_count[i] = 0;
    }

    ECB_pcidev_entry_list(pecb) = (DRV_PCI_DEVICE_ENTRY)((S8*)pecb + ECB_pcidev_list_offset(pecb));
    dpden = ECB_pcidev_entry_list(pecb);

    if (counter_virtual_address) {
        for (i = 0; i < ECB_num_entries(pecb); i++) {
            writel(HSWUNC_SA_CHAP_STOP,
                (U32*)(((char*)(UIOP)counter_virtual_address)+HSWUNC_SA_CHAP_CTRL_REG_OFFSET+i*0x10));
        }
    }

    for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
        curr_pci_entry = &dpden[dev_index];
        mmio_offset    = DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(curr_pci_entry);
        bar_name       = DRV_PCI_DEVICE_ENTRY_bar_name(curr_pci_entry);
        if (DRV_PCI_DEVICE_ENTRY_config_type(curr_pci_entry) == UNC_PCICFG) {
            pci_address = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                        mmio_offset);
            PCI_Write_Ulong(pci_address, DRV_PCI_DEVICE_ENTRY_value(curr_pci_entry));
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
        if (bar_name == UNC_GDXCBAR) {
            DRV_PCI_DEVICE_ENTRY_bar_address(curr_pci_entry) = gdxc_bar;
        }
        else {
            pci_address     = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_bar_offset(curr_pci_entry));
            bar_lo          = PCI_Read_Ulong(pci_address);
            next_bar_offset = DRV_PCI_DEVICE_ENTRY_bar_offset(curr_pci_entry)+HSWUNC_SA_NEXT_ADDR_OFFSET;
            pci_address     = FORM_PCI_ADDR(DRV_PCI_DEVICE_ENTRY_bus_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_dev_no(curr_pci_entry),
                                        DRV_PCI_DEVICE_ENTRY_func_no(curr_pci_entry),
                                        next_bar_offset);
            bar_hi      = PCI_Read_Ulong(pci_address);
            final_bar   = (bar_hi << HSWUNC_SA_BAR_ADDR_SHIFT) | bar_lo;
            final_bar  &= HSWUNC_SA_BAR_ADDR_MASK;

            DRV_PCI_DEVICE_ENTRY_bar_address(curr_pci_entry) = final_bar;
        }
        physical_address = DRV_PCI_DEVICE_ENTRY_bar_address(curr_pci_entry);

        if (physical_address) {
            if (bar_name == UNC_MCHBAR) {
                map_size = HSWUNC_SA_MCHBAR_MMIO_PAGE_SIZE;
            }
            else if (bar_name == UNC_PCIEXBAR) {
                map_size = HSWUNC_SA_PCIEXBAR_MMIO_PAGE_SIZE;
            }
            else {
                map_size = HSWUNC_SA_OTHER_BAR_MMIO_PAGE_SIZE;
            }
            DRV_PCI_DEVICE_ENTRY_virtual_address(curr_pci_entry) = (U64) (UIOP)ioremap_nocache(physical_address, map_size);
            virtual_address = DRV_PCI_DEVICE_ENTRY_virtual_address(curr_pci_entry);

            if (!gdxc_bar && bar_name == UNC_MCHBAR) {
                bar_lo   = readl((U32*)((char*)(UIOP)virtual_address+HSWUNC_SA_GDXCBAR_OFFSET_LO));
                bar_hi   = readl((U32*)((char*)(UIOP)virtual_address+HSWUNC_SA_GDXCBAR_OFFSET_HI));
                gdxc_bar = (bar_hi << HSWUNC_SA_BAR_ADDR_SHIFT) | bar_lo;
                gdxc_bar = gdxc_bar & HSWUNC_SA_GDXCBAR_MASK;
            }
            writel((U32)DRV_PCI_DEVICE_ENTRY_value(curr_pci_entry), (U32*)(((char*)(UIOP)virtual_address)+mmio_offset));
            bar_list[bar_name] = dev_index;
            if (counter_virtual_address == 0 && bar_name == UNC_CHAPADR) {
                counter_virtual_address = virtual_address;
            }
        }
    }

    return;
}

/*!
 * @fn         static VOID hswunc_sa_Disable_PMU(PVOID)
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
hswunc_sa_Disable_PMU (
    PVOID  param
)
{

    DRV_PCI_DEVICE_ENTRY  dpden;
    U32                   dev_index = 0;
    U32                   cur_grp   = LWPMU_DEVICE_cur_group(device_uncore);
    ECB                   pecb      = LWPMU_DEVICE_PMU_register_data(device_uncore)[cur_grp];
    U32                   i         = 0;

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP) {

        if (counter_virtual_address) {
            for (i = 0; i < ECB_num_entries(pecb); i++) {
                writel(HSWUNC_SA_CHAP_STOP,
                    (U32*)(((char*)(UIOP)counter_virtual_address)+HSWUNC_SA_CHAP_CTRL_REG_OFFSET+i*0x10));
            }
        }

        dpden = ECB_pcidev_entry_list(pecb);
        for (dev_index = 0; dev_index < ECB_num_pci_devices(pecb); dev_index++) {
            if (DRV_PCI_DEVICE_ENTRY_config_type(&dpden[dev_index]) == UNC_MMIO &&
                DRV_PCI_DEVICE_ENTRY_bar_address(&dpden[dev_index]) != 0) {
                iounmap((void*)(UIOP)(DRV_PCI_DEVICE_ENTRY_virtual_address(&dpden[dev_index])));
            }
        }
        counter_virtual_address = 0;
    }

    return;
}

/*!
 * @fn         static VOID hswunc_sa_Initialize(PVOID)
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
hswunc_sa_Initialize (
    VOID  *param
)
{
    counter_virtual_address = 0;
    return;
}


/*!
 * @fn         static VOID hswunc_sa_Clean_Up(PVOID)
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
hswunc_sa_Clean_Up (
    VOID   *param
)
{
    counter_virtual_address = 0;
    return;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn hswunc_sa_Read_Data(param, id)
 *
 * @param    data_buffer    data buffer to read data into
 *
 * @return   None     No return needed
 *
 * @brief    Read the Uncore count data and store into the buffer param;
 *
 */
static VOID
hswunc_sa_Read_Data (
    PVOID data_buffer
)
{
    U32                   event_id    = 0;
    U64                  *data;
    int                   data_index;
    U32                   data_val    = 0;
    U64                   total_count = 0;
    U32                   cur_grp     = LWPMU_DEVICE_cur_group(device_uncore);

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_IDLE          ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RESERVED      ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP  ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
        SOCPERF_PRINT_ERROR("ERROR: RETURING EARLY from Read_Data\n");
        return;
    }
    if (data_buffer == NULL) {
        return;
    }
    data       = (U64 *)data_buffer;
    data_index = 0;
    // group id
    data[data_index] = cur_grp + 1;
    data_index++;

    FOR_EACH_PCI_DATA_REG_RAW(pecb, i, dev_idx) {
        //event_id = ECB_entries_event_id_index_local(pecb, i);
        if (counter_virtual_address) {
            writel(HSWUNC_SA_CHAP_SAMPLE_DATA,
               (U32*)(((char*)(UIOP)counter_virtual_address) + HSWUNC_SA_CHAP_CTRL_REG_OFFSET + i*0x10));
            data_val = readl((U32*)((char*)(UIOP)(counter_virtual_address) + ECB_entries_pci_id_offset(pecb, i)));
        }

        if (data_val < pcb[0].last_uncore_count[i]) {
            counter_overflow[i]++;
        }
        pcb[0].last_uncore_count[i] = data_val;

        total_count = data_val + counter_overflow[i]*HSWUNC_SA_MAX_COUNT;
        data[data_index+event_id] = total_count;
        SOCPERF_PRINT_DEBUG("DATA=%u\n", data_val);
        event_id++;
    } END_FOR_EACH_PCI_DATA_REG_RAW;

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  hswunc_sa_dispatch =
{
    hswunc_sa_Initialize,        // initialize
    NULL,                        // destroy
    hswunc_sa_Write_PMU ,        // write
    hswunc_sa_Disable_PMU,       // freeze
    NULL,                        // restart
    NULL,                        // read
    NULL,                        // check for overflow
    NULL,
    NULL,
    hswunc_sa_Clean_Up,
    NULL,
    NULL,
    NULL,
    NULL,                        //read_counts
    NULL,
    NULL,
    NULL,
    NULL,
    hswunc_sa_Read_Data,
    NULL,
    NULL,
    NULL,
    NULL
};

