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
#include <linux/percpu.h>

#include "lwpmudrv_defines.h"
#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"
#include "lwpmudrv_chipset.h"
#include "inc/lwpmudrv.h"
#include "inc/control.h"
#include "inc/ecb_iterators.h"
#include "inc/utility.h"

extern DRV_CONFIG         pcfg;
extern CHIPSET_CONFIG     pma;
extern CPU_STATE          pcb;
extern EVENT_CONFIG       global_ec;

/* ------------------------------------------------------------------------- */
/*!
 * @fn          static U32 chap_Init_Chipset(void)
 * 
 * @brief       Chipset PMU initialization
 *
 * @param       None
 * 
 * @return      VT_SUCCESS if successful, otherwise error
 *
 * <I>Special Notes:</I>
 *             <NONE>
 */
static U32
chap_Init_Chipset (
    VOID
)
{
    U32 i;
    CHIPSET_SEGMENT mch_chipset_seg = &CHIPSET_CONFIG_mch(pma);
    CHIPSET_SEGMENT ich_chipset_seg = &CHIPSET_CONFIG_ich(pma);
    CHIPSET_SEGMENT noa_chipset_seg = &CHIPSET_CONFIG_noa(pma);

    SEP_PRINT_DEBUG("Initializing chipset ...\n");

    if (DRV_CONFIG_enable_chipset(pcfg)) {
        for (i=0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
            pcb[i].chipset_count_init = TRUE;
        }
        if (CHIPSET_CONFIG_mch_chipset(pma)) {
            if (CHIPSET_SEGMENT_virtual_address(mch_chipset_seg) == 0) {
                // Map the virtual address of the PCI CHAP interface.
                CHIPSET_SEGMENT_virtual_address(mch_chipset_seg) = (U64) (UIOP) ioremap_nocache(
                                                      CHIPSET_SEGMENT_physical_address(mch_chipset_seg),
                                                      CHIPSET_SEGMENT_size(mch_chipset_seg));
            }
        }

        if (CHIPSET_CONFIG_ich_chipset(pma)) {
            if (CHIPSET_SEGMENT_virtual_address(ich_chipset_seg) == 0) {
                // Map the virtual address of the PCI CHAP interface.
                CHIPSET_SEGMENT_virtual_address(ich_chipset_seg) = (U64) (UIOP) ioremap_nocache(
                                                      CHIPSET_SEGMENT_physical_address(ich_chipset_seg),
                                                      CHIPSET_SEGMENT_size(ich_chipset_seg));
            }
        }

        // Here we map the MMIO registers for the Gen X processors.
        if (CHIPSET_CONFIG_noa_chipset(pma)) {
            if (CHIPSET_SEGMENT_virtual_address(noa_chipset_seg) == 0) {
                // Map the virtual address of the PCI CHAP interface.
                CHIPSET_SEGMENT_virtual_address(noa_chipset_seg) = (U64) (UIOP) ioremap_nocache(
                                                    CHIPSET_SEGMENT_physical_address(noa_chipset_seg),
                                                    CHIPSET_SEGMENT_size(noa_chipset_seg));
            }
        }

        //
        // always collect processor events
        //
        CHIPSET_CONFIG_processor(pma) = 1;
    }
    else {
        CHIPSET_CONFIG_processor(pma) = 0;
    }
    SEP_PRINT_DEBUG("Initializing chipset done.\n");

    return VT_SUCCESS;
}



/* ------------------------------------------------------------------------- */
/*!
 * @fn          static U32 chap_Start_Chipset(void)
 * @param       None
 * @return      VT_SUCCESS if successful, otherwise error
 * @brief       Start collection on the Chipset PMU
 *
 * <I>Special Notes:</I>
 *             <NONE>
 */
static VOID
chap_Start_Chipset (
    VOID
)
{
    U32 i;
    CHAP_INTERFACE  chap;
    CHIPSET_SEGMENT mch_chipset_seg = &CHIPSET_CONFIG_mch(pma);
    CHIPSET_SEGMENT ich_chipset_seg = &CHIPSET_CONFIG_ich(pma);

    //
    // reset and start chipset counters
    //
    SEP_PRINT_DEBUG("Starting chipset counters...\n");
    if (pma) {
        chap = (CHAP_INTERFACE)(UIOP)CHIPSET_SEGMENT_virtual_address(mch_chipset_seg);
        if (chap != NULL) {
            for (i = 0; i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
                CHAP_INTERFACE_command_register(&chap[i]) = 0x00040000; // Reset to zero
                CHAP_INTERFACE_command_register(&chap[i]) = 0x00010000; // Restart
            }
        }

        chap = (CHAP_INTERFACE) (UIOP)CHIPSET_SEGMENT_virtual_address(ich_chipset_seg);
        if (chap != NULL) {
            for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
                CHAP_INTERFACE_command_register(&chap[i]) = 0x00040000; // Reset to zero
                CHAP_INTERFACE_command_register(&chap[i]) = 0x00010000; // Restart
            }
        }
    }

    SEP_PRINT_DEBUG("Starting chipset counters done.\n");

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          static U32 chap_Read_Counters(PVOID param)
 * 
 * @brief       Read the CHAP counter data
 *
 * @param       PVOID param - address of the buffer to write into
 * 
 * @return      None
 *
 * <I>Special Notes:</I>
 *             <NONE>
 */
static VOID
chap_Read_Counters (
    PVOID  param
)
{
    U64            *data;
    CHAP_INTERFACE  chap;
    U32             mch_cpu;
    int             i, data_index;
    U64             tmp_data;
    U64            *mch_data;
    U64            *ich_data;
    U64            *mmio_data;
    U64            *mmio;
    U32             this_cpu        = CONTROL_THIS_CPU();
    CHIPSET_SEGMENT mch_chipset_seg = &CHIPSET_CONFIG_mch(pma);
    CHIPSET_SEGMENT ich_chipset_seg = &CHIPSET_CONFIG_ich(pma);
    CHIPSET_SEGMENT noa_chipset_seg = &CHIPSET_CONFIG_noa(pma);

    data       = param;
    data_index = 0;

    // Save the Motherboard time.  This is universal time for this
    // system.  This is the only 64-bit timer so we save it first so
    // always aligned on 64-bit boundary that way.

    if (CHIPSET_CONFIG_mch_chipset(pma)) {
        mch_data = data + data_index;
        // Save the MCH counters.
        chap = (CHAP_INTERFACE)(UIOP)CHIPSET_SEGMENT_virtual_address(mch_chipset_seg);
        for (i = CHIPSET_SEGMENT_start_register(mch_chipset_seg);
                        i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
            CHAP_INTERFACE_command_register(&chap[i]) = 0x00020000; // Sample
        }

        // The StartingReadRegister is only used for special event
        // configs that use CHAP counters to trigger events in other
        // CHAP counters.  This is an unusual request but useful in
        // getting the number of lit subspans - implying a count of the
        // number of triangles.  I am not sure it will be used
        // elsewhere.  We cannot read some of the counters because it
        // will invalidate their configuration to trigger other CHAP
        // counters.  Yuk!
        data_index += CHIPSET_SEGMENT_start_register(mch_chipset_seg);
        for (i = CHIPSET_SEGMENT_start_register(mch_chipset_seg);
                        i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
            data[data_index++] = CHAP_INTERFACE_data_register(&chap[i]);
        }

        // Initialize the counters on the first interrupt
        if (pcb[this_cpu].chipset_count_init == TRUE) {
            for (i = 0; i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
                pcb[this_cpu].last_mch_count[i] = mch_data[i];
            }
        }

        // Now compute the delta!
        // NOTE: Special modification to accomodate Gen 4 work - count
        // everything since last interrupt - regardless of cpu!  This
        // way there is only one count of the Gen 4 counters.
        //
        mch_cpu = CHIPSET_CONFIG_host_proc_run(pma) ? this_cpu : 0;
        for (i = 0; i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
            tmp_data = mch_data[i];
            if (mch_data[i] < pcb[mch_cpu].last_mch_count[i]) {
                mch_data[i] = mch_data[i] + (U32)(-1) - pcb[mch_cpu].last_mch_count[i];
            }
            else {
                mch_data[i] = mch_data[i] - pcb[mch_cpu].last_mch_count[i];
            }
            pcb[mch_cpu].last_mch_count[i] = tmp_data;
        }
    }

    if (CHIPSET_CONFIG_ich_chipset(pma)) {
        // Save the ICH counters.
        ich_data = data + data_index;
        chap = (CHAP_INTERFACE)(UIOP)CHIPSET_SEGMENT_virtual_address(ich_chipset_seg);
        for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
            CHAP_INTERFACE_command_register(&chap[i]) = 0x00020000; // Sample
        }

        for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
            data[data_index++] = CHAP_INTERFACE_data_register(&chap[i]);
        }

        // Initialize the counters on the first interrupt
        if (pcb[this_cpu].chipset_count_init == TRUE) {
            for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
                pcb[this_cpu].last_ich_count[i] = ich_data[i];
            }
        }

        // Now compute the delta!
        for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
            tmp_data = ich_data[i];
            if (ich_data[i] < pcb[this_cpu].last_ich_count[i]) {
                ich_data[i] = ich_data[i] + (U32)(-1) - pcb[this_cpu].last_ich_count[i];
            }
            else {
                ich_data[i] = ich_data[i] - pcb[this_cpu].last_ich_count[i];
            }
            pcb[this_cpu].last_ich_count[i] = tmp_data;
        }
    }

    if (CHIPSET_CONFIG_noa_chipset(pma)) {
        // Save the MMIO counters.
        mmio_data = data + data_index;
        mmio      = (U64 *) (UIOP)CHIPSET_SEGMENT_virtual_address(noa_chipset_seg);

        for (i = 0; i < CHIPSET_SEGMENT_total_events(noa_chipset_seg); i++) {
            data[data_index++] = mmio[i*2 + 2244]; // 64-bit quantity
        }

        // Initialize the counters on the first interrupt
        if (pcb[this_cpu].chipset_count_init == TRUE) {
            for (i = 0; i < CHIPSET_SEGMENT_total_events(noa_chipset_seg); i++) {
                pcb[this_cpu].last_mmio_count[i] = mmio_data[i];
            }
        }

        // Now compute the delta!
        for (i = 0; i < CHIPSET_SEGMENT_total_events(noa_chipset_seg); i++) {
            tmp_data = mmio_data[i];
            if (mmio_data[i] < pcb[this_cpu].last_mmio_count[i]) {
                mmio_data[i] = mmio_data[i] + (U32)(-1) - pcb[this_cpu].last_mmio_count[i];
            }
            else {
                mmio_data[i] = mmio_data[i] - pcb[this_cpu].last_mmio_count[i];
            }
            pcb[this_cpu].last_mmio_count[i] = tmp_data;
        }
    }

    pcb[this_cpu].chipset_count_init = FALSE;

    FOR_EACH_DATA_REG(pecb,i) {
            data[data_index++] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), (U64)0);
    } END_FOR_EACH_DATA_REG;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          static VOID chap_Stop_Chipset(void)
 * 
 * @brief       Stop the Chipset PMU
 *
 * @param       None
 * 
 * @return      None
 *
 * <I>Special Notes:</I>
 *             <NONE>
 */
static VOID
chap_Stop_Chipset (
    VOID
)
{
    U32 i;
    CHAP_INTERFACE  chap;
    CHIPSET_SEGMENT mch_chipset_seg = &CHIPSET_CONFIG_mch(pma);
    CHIPSET_SEGMENT ich_chipset_seg = &CHIPSET_CONFIG_ich(pma);

    //
    // reset and start chipset counters
    //
    SEP_PRINT_DEBUG("Stopping chipset counters...\n");
    if (pma) {
        if (CHIPSET_CONFIG_mch_chipset(pma)) {
            chap = (CHAP_INTERFACE)(UIOP)CHIPSET_SEGMENT_virtual_address(mch_chipset_seg);
            if (chap != NULL) {
                for (i = 0; i < CHIPSET_SEGMENT_total_events(mch_chipset_seg); i++) {
                    CHAP_INTERFACE_command_register(&chap[i]) = 0x00000000; // Stop
                    CHAP_INTERFACE_command_register(&chap[i]) = 0x00040000; // Reset to Zero
                }
            }
        }

        if (CHIPSET_CONFIG_ich_chipset(pma)) {
            chap = (CHAP_INTERFACE)(UIOP)CHIPSET_SEGMENT_virtual_address(ich_chipset_seg);
            if (chap != NULL) {
                for (i = 0; i < CHIPSET_SEGMENT_total_events(ich_chipset_seg); i++) {
                    CHAP_INTERFACE_command_register(&chap[i]) = 0x00000000; // Stop
                    CHAP_INTERFACE_command_register(&chap[i]) = 0x00040000; // Reset to Zero
                }
            }
        }

        if (CHIPSET_CONFIG_mch_chipset(pma) && CHIPSET_SEGMENT_virtual_address(mch_chipset_seg)) {
            iounmap((void*)(UIOP)CHIPSET_SEGMENT_virtual_address(mch_chipset_seg));
            CHIPSET_SEGMENT_virtual_address(mch_chipset_seg) = 0;
        }

        if (CHIPSET_CONFIG_ich_chipset(pma) && CHIPSET_SEGMENT_virtual_address(ich_chipset_seg)) {
            iounmap((void*)(UIOP)CHIPSET_SEGMENT_virtual_address(ich_chipset_seg));
            CHIPSET_SEGMENT_virtual_address(ich_chipset_seg) = 0;
        }
        CONTROL_Free_Memory(pma);
        pma = NULL;
    }

    SEP_PRINT_DEBUG("Stopped chipset counters.\n");

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn          static VOID chap_Fini_Chipset(void)
 * 
 * @brief       Finish routine on a per-logical-core basis
 *
 * @param       None
 * 
 * @return      None
 *
 * <I>Special Notes:</I>
 *             <NONE>
 */
static VOID
chap_Fini_Chipset (
    VOID
)
{
    return;
}

CS_DISPATCH_NODE  chap_dispatch =
{
    chap_Init_Chipset,
    chap_Start_Chipset,
    chap_Read_Counters,
    chap_Stop_Chipset,
    chap_Fini_Chipset,
    NULL
};
