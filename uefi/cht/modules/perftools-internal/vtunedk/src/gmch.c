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

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include "lwpmudrv_defines.h"
#include "lwpmudrv_types.h"

#if defined(PCI_HELPERS_API)
#include <asm/intel_scu_ipc.h>
#include <asm/intel-mid.h>
#endif

#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv_chipset.h"
#include "inc/lwpmudrv.h"
#include "inc/control.h"
#include "inc/utility.h"
#include "inc/ecb_iterators.h"
#include "inc/gmch.h"
#include "inc/pci.h"

// global variables for determining which register offsets to use
static U32 gmch_register_read  = 0;     // value=0 indicates invalid read register
static U32 gmch_register_write = 0;     // value=0 indicates invalid write register
static U32 number_of_events    = 0;

//global variable for reading GMCH counter values
static U64              *gmch_current_data = NULL;
static U64              *gmch_to_read_data = NULL;

// global variable for tracking number of overflows per GMCH counter
static U32               gmch_overflow[MAX_CHIPSET_COUNTERS];
static U64               last_gmch_count[MAX_CHIPSET_COUNTERS];

extern DRV_CONFIG        pcfg;
extern CHIPSET_CONFIG    pma;
extern CPU_STATE         pcb;
extern EVENT_CONFIG      global_ec;

/*
 * @fn        gmch_PCI_Read32(address)
 *
 * @brief     Read the 32bit value specified by the address
 *
 * @return    the read value
 *
 */
#if defined(PCI_HELPERS_API)
#define gmch_PCI_Read32   intel_mid_msgbus_read32_raw
#else
static U32
gmch_PCI_Read32 (
    unsigned long address
)
{
    U32 read_value = 0;
    U32 gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch == 0) {
        return 0;
    }
    PCI_Write_Ulong((ULONG)(gmch + GMCH_MSG_CTRL_REG), (ULONG)address);
    read_value = PCI_Read_Ulong((ULONG)(gmch + GMCH_MSG_DATA_REG));

    return read_value;
}
#endif

/*
 * @fn        gmch_PCI_Write32(address, data)
 *
 * @brief     Write the 32bit value into the address specified
 *
 * @return    None
 *
 */
#if defined(PCI_HELPERS_API)
#define gmch_PCI_Write32  intel_mid_msgbus_write32_raw
#else
static void
gmch_PCI_Write32 (
    unsigned long address,
    unsigned long data
)
{
    U32 gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch == 0) {
        return;
    }
    PCI_Write_Ulong(gmch + GMCH_MSG_DATA_REG, data);
    PCI_Write_Ulong(gmch + GMCH_MSG_CTRL_REG, address);
    return;
}
#endif

/*
 * @fn        gmch_Check_Enabled()
 *
 * @brief     Read GMCH PMON capabilities
 *
 * @param     None
 *
 * @return    GMCH enable bits
 *
 */
static ULONG
gmch_Check_Enabled (
    VOID
)
{
    U32   gmch;
    ULONG enabled_value;

    gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch == 0) {
        SEP_PRINT_ERROR("gmch_Check_Enabled: unable to access PCI config space!\n");
        return 0;
    }

    SEP_PRINT_DEBUG("gmch_Check_Enabled: wrote addr=0x%lx register_value=0x%lx\n", (ULONG)(gmch+GMCH_MSG_CTRL_REG), (ULONG)(GMCH_PMON_CAPABILITIES+gmch_register_read));

    enabled_value = gmch_PCI_Read32(GMCH_PMON_CAPABILITIES + gmch_register_read);

    SEP_PRINT_DEBUG("gmch_Check_Enabled: read addr=0x%lx enabled_value=0x%lx\n", (ULONG)(gmch+GMCH_MSG_DATA_REG), enabled_value);

    return enabled_value;
}

/*
 * @fn        gmch_Init_Chipset()
 *
 * @brief     Initialize GMCH Counters.  See note below.
 *
 * @param     None
 *
 * @note      This function must be called BEFORE any other function in this file!
 *
 * @return    VT_SUCCESS if successful, error otherwise
 *
 */
static U32
gmch_Init_Chipset (
    VOID
)
{
    int             i;
    U32             gmch;
    CHIPSET_SEGMENT cs = &CHIPSET_CONFIG_gmch(pma);
    CHIPSET_SEGMENT gmch_chipset_seg;    

    gmch_chipset_seg = &CHIPSET_CONFIG_gmch(pma);

    // configure the read/write registers offsets according to usermode setting
    if (cs) {
        gmch_register_read = CHIPSET_SEGMENT_read_register(cs);
        gmch_register_write = CHIPSET_SEGMENT_write_register(cs);;
    }
    if (gmch_register_read == 0 || gmch_register_write == 0) {
        SEP_PRINT_ERROR("Invalid GMCH read/write registers!\n");
        return VT_CHIPSET_CONFIG_FAILED;
    }

    number_of_events = CHIPSET_SEGMENT_total_events(gmch_chipset_seg);
    SEP_PRINT("Number of chipset events %d\n", number_of_events);

    // Allocate memory for reading GMCH counter values + the group id
    gmch_current_data = CONTROL_Allocate_Memory((number_of_events+1)*sizeof(U64));
    if (!gmch_current_data) {
        return OS_NO_MEM;
    }
    gmch_to_read_data = CONTROL_Allocate_Memory((number_of_events+1)*sizeof(U64));
    if (!gmch_to_read_data) {
        return OS_NO_MEM;
    }

    if (!DRV_CONFIG_enable_chipset(pcfg)) {
        return VT_SUCCESS;
    }

    if (!CHIPSET_CONFIG_gmch_chipset(pma)) {
        return VT_SUCCESS;
    }
    // initialize the GMCH per-counter overflow numbers
    for (i = 0; i < MAX_CHIPSET_COUNTERS; i++) {
        gmch_overflow[i]   = 0;
        last_gmch_count[i] = 0;
    }
    gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch == 0) {
        return VT_SUCCESS;
    }

    // disable fixed and GP counters
    gmch_PCI_Write32(GMCH_PMON_GLOBAL_CTRL+gmch_register_write, 0x00000000);
    // clear fixed counter filter
    gmch_PCI_Write32(GMCH_PMON_FIXED_CTR_CTRL+gmch_register_write, 0x00000000);

    return VT_SUCCESS;
}

/*
 * @fn        gmch_Start_Counters()
 *
 * @brief     Start the GMCH Counters.
 *
 * @param     None
 *
 * @return    None
 *
 */
static VOID
gmch_Start_Counters (
    VOID
)
{
    U32 gmch;
    // reset and start chipset counters
    if (pma == NULL) {
        SEP_PRINT_ERROR("gmch_Start_Counters: ERROR pma=NULL\n");
    }
    gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch != 0) {
        // enable fixed and GP counters
        gmch_PCI_Write32(GMCH_PMON_GLOBAL_CTRL+gmch_register_write, 0x0001000F);
        // enable fixed counter filter
        gmch_PCI_Write32(GMCH_PMON_FIXED_CTR_CTRL+gmch_register_write, 0x00000001);
    }

    return;
}

/*
 * @fn        gmch_Trigger_Read()
 *
 * @brief     Read the GMCH counters through PCI Config space
 *
 * @return    None
 *
 */
static VOID
gmch_Trigger_Read (
    VOID
)
{
    U64            *data;
    U32             gmch;
    int             i, data_index;
    U64             val;
    U64            *gmch_data;
    U32             counter_data_low;
    U32             counter_data_high;
    U64             counter_data;
    U64             cmd_register_low_read;
    U64             cmd_register_high_read;
    U32             gp_counter_index    = 0;
    U64             overflow;

    CHIPSET_SEGMENT gmch_chipset_seg;
    CHIPSET_EVENT   chipset_events;
    U64             *temp;


    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_IDLE          ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RESERVED      ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP  ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
        return;
    }

    if (pma == NULL) {
        return;
    }

    if (gmch_current_data == NULL) {
        return;
    }

    if (CHIPSET_CONFIG_gmch_chipset(pma) == 0) {
        return;
    }

    data       = gmch_current_data;
    data_index = 0;

    preempt_disable();
    SYS_Local_Irq_Disable();
    gmch_chipset_seg    = &CHIPSET_CONFIG_gmch(pma);
    chipset_events      = CHIPSET_SEGMENT_events(gmch_chipset_seg);

    // Write GroupID
    data[data_index] = 1;
    // Increment the data index as the event id starts from zero
    data_index++;

    // GMCH data will be written as gmch_data[0], gmch_data[1], ...
    gmch_data = data + data_index;

    // read the GMCH counters and add them into the sample record
    gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch == 0) {
        return;
    }

    // iterate through GMCH counters that were configured to collect on the events
    for (i = 0; i < CHIPSET_SEGMENT_total_events(gmch_chipset_seg); i++) {
        U32 event_id = CHIPSET_EVENT_event_id(&chipset_events[i]);
        // read count for fixed GMCH counter event
        if (event_id == 0) {
            cmd_register_low_read  = GMCH_PMON_FIXED_CTR0 + gmch_register_read;
            data[data_index++]     = (U64)gmch_PCI_Read32(cmd_register_low_read);
            overflow               = GMCH_PMON_FIXED_CTR_OVF_VAL;
        }
        else {
            // read count for general GMCH counter event
            switch (gp_counter_index) {
                case 0:
                default:
                    cmd_register_low_read  = GMCH_PMON_GP_CTR0_L + gmch_register_read;
                    cmd_register_high_read = GMCH_PMON_GP_CTR0_H + gmch_register_read;
                    break;

                case 1:
                    cmd_register_low_read  = GMCH_PMON_GP_CTR1_L + gmch_register_read;
                    cmd_register_high_read = GMCH_PMON_GP_CTR1_H + gmch_register_read;
                    break;

                case 2:
                    cmd_register_low_read  = GMCH_PMON_GP_CTR2_L + gmch_register_read;
                    cmd_register_high_read = GMCH_PMON_GP_CTR2_H + gmch_register_read;
                    break;

                case 3:
                    cmd_register_low_read  = GMCH_PMON_GP_CTR3_L + gmch_register_read;
                    cmd_register_high_read = GMCH_PMON_GP_CTR3_H + gmch_register_read;
                    break;
            }
            counter_data_low   = gmch_PCI_Read32(cmd_register_low_read);
            counter_data_high  = gmch_PCI_Read32(cmd_register_high_read);
            counter_data       = (U64)counter_data_high;
            data[data_index++] = (counter_data << 32) + counter_data_low;
            overflow           = GMCH_PMON_GP_CTR_OVF_VAL;
            gp_counter_index++;
        }

        /* Compute the running count of the event. */
        gmch_data[i] &= overflow;
        val           = gmch_data[i];
        if (gmch_data[i] < last_gmch_count[i]) {
            gmch_overflow[i]++;
        }
        gmch_data[i]       = gmch_data[i] + gmch_overflow[i]*overflow;
        last_gmch_count[i] = val;
    }

    temp              = gmch_to_read_data;
    gmch_to_read_data = gmch_current_data;
    gmch_current_data = temp;
    SYS_Local_Irq_Enable();
    preempt_enable();

    return;
}

/*
 * @fn        gmch_Read_Counters()
 *
 * @brief     Copy the GMCH data to the sampling data stream.
 *
 * @param     param - pointer to data stream where samples are to be written
 *
 * @return    None
 *
 */
static VOID
gmch_Read_Counters (
    PVOID  param
)
{
    U64            *data;
    int             i;

    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_UNINITIALIZED ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_IDLE          ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RESERVED      ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_PREPARE_STOP  ||
        GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_STOPPED) {
        return;
    }

    if (pma == NULL) {
        return;
    }

    if (param == NULL) {
        return;
    }

    if (gmch_to_read_data == NULL) {
        return;
    }

    /*
     * Account for the group id that is placed at the start of the chipset array.
     * The number of data elements to be transferred is number_of_events + 1.
     */
    data = param;
    for (i = 0; i < number_of_events+1; i++) {
         data[i] = gmch_to_read_data[i];    
         SEP_PRINT_DEBUG("Interrupt gmch read counters data %d is: 0x%llx \n",i, data[i]);
    }

    return;
}

/*
 * @fn        gmch_Stop_Counters()
 *
 * @brief     Stop the GMCH counters
 *
 * @param     None
 *
 * @return    None
 *
 */
static VOID
gmch_Stop_Counters (
    VOID
)
{
    U32 gmch;
    // stop and reset the chipset counters
    number_of_events = 0;
    if (pma == NULL) {
        SEP_PRINT_ERROR("gmch_Stop_Counters: pma=NULL\n");
    }
    gmch = FORM_PCI_ADDR(0, 0, 0, 0);
    if (gmch != 0) {
        // disable fixed and GP counters
        gmch_PCI_Write32(GMCH_PMON_GLOBAL_CTRL+gmch_register_write, 0x00000000);
        gmch_PCI_Write32(GMCH_PMON_FIXED_CTR_CTRL+gmch_register_write, 0x00000000);
    }

    return;
}

/*
 * @fn        gmch_Fini_Chipset()
 *
 * @brief     Reset GMCH to state where it can be used again.  Called at cleanup phase.
 *
 * @param     None
 *
 * @return    None
 *
 */
static VOID
gmch_Fini_Chipset (
    VOID
)
{
    if (!gmch_Check_Enabled()) {
        SEP_PRINT_WARNING("GMCH is not enabled!\n");
    }

    gmch_current_data = CONTROL_Free_Memory(gmch_current_data);
    gmch_to_read_data = CONTROL_Free_Memory(gmch_to_read_data);

    return;
}

//
// Initialize the GMCH chipset dispatch table
//
CS_DISPATCH_NODE gmch_dispatch =
{
    gmch_Init_Chipset,
    gmch_Start_Counters,
    gmch_Read_Counters,
    gmch_Stop_Counters,
    gmch_Fini_Chipset,
    gmch_Trigger_Read
};
