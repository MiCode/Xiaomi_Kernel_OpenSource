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
#include <linux/fs.h>
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include <asm/msr.h>
#endif
#include <linux/ptrace.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "core2.h"
#include "perfver4.h"
#include "silvermont.h"
#include "valleyview_sochap.h"
#include "avt_unc_power.h"
#include "snb_unc_cbo.h"
#include "snb_unc_imc.h"
#include "snb_unc_power.h"
#include "snb_unc_gt.h"
#include "hsw_unc_ncu.h"
#include "hsw_unc_power.h"
#if !defined (DRV_ANDROID)
#include "corei7_unc.h"
#include "wsx_unc_imc.h"
#include "wsx_unc_qpi.h"
#include "wsx_unc_wbox.h"
#include "jkt_unc_imc.h"
#include "jkt_unc_qpill.h"
#include "jkt_unc_ubox.h"
#include "ivt_unc_cbo.h"
#include "ivt_unc_imc.h"
#include "ivt_unc_pcu.h"
#include "ivt_unc_ha.h"
#include "ivt_unc_qpill.h"
#include "ivt_unc_r3qpi.h"
#include "ivt_unc_ubox.h"
#include "ivt_unc_r2pcie.h"
#include "ivt_unc_irp.h"
#include "hsx_unc_cbo.h"
#include "hsx_unc_imc.h"
#include "hsx_unc_pcu.h"
#include "hsx_unc_ha.h"
#include "hsx_unc_qpill.h"
#include "hsx_unc_r3qpi.h"
#include "hsx_unc_ubox.h"
#include "hsx_unc_r2pcie.h"
#include "hsx_unc_sbox.h"
#include "hsx_unc_irp.h"
#include "skl_unc_cbo.h"
#include "skl_unc_ncu.h"
#include "chap.h"
#endif
#endif
#include "utility.h"
#include "lwpmudrv_chipset.h"
#include "gmch.h"

volatile int config_done;

extern CHIPSET_CONFIG pma;

extern DRV_BOOL
UTILITY_down_read_mm (
    struct task_struct *p
)
{
#ifdef SUPPORTS_MMAP_READ
    mmap_down_read(p->mm);
#else
    down_read((struct rw_semaphore *) &p->mm->mmap_sem);
#endif
    return TRUE;
}

extern VOID
UTILITY_up_read_mm (
    struct task_struct *p
)
{
#ifdef SUPPORTS_MMAP_READ
    mmap_up_read(p->mm);
#else
    up_read((struct rw_semaphore *) &p->mm->mmap_sem);
#endif

    return;
}

extern VOID
UTILITY_Read_TSC (
    U64* pTsc
)
{
#if defined(DRV_IA32) || defined(DRV_EM64T)
    rdtscll(*(pTsc));
#else
    *(pTsc) = itp_get_itc();
#endif

    return;
}

#if defined(DRV_IA32) || defined(DRV_EM64T)
/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Read_Cpuid
 *
 * @brief    executes the cpuid_function of cpuid and returns values
 *
 * @param  IN   cpuid_function
 *         OUT  rax  - results of the cpuid instruction in the
 *         OUT  rbx  - corresponding registers
 *         OUT  rcx
 *         OUT  rdx
 *
 * @return   none
 *
 * <I>Special Notes:</I>
 *              <NONE>
 *
 */
extern VOID
UTILITY_Read_Cpuid (
    U64   cpuid_function,
    U64  *rax_value,
    U64  *rbx_value,
    U64  *rcx_value,
    U64  *rdx_value
)
{
    U32 function = (U32) cpuid_function;
    U32 *eax     = (U32 *) rax_value;
    U32 *ebx     = (U32 *) rbx_value;
    U32 *ecx     = (U32 *) rcx_value;
    U32 *edx     = (U32 *) rdx_value;

    *eax = function;

    __asm__("cpuid"
            : "=a" (*eax),
              "=b" (*ebx),
              "=c" (*ecx),
              "=d" (*edx)
            : "a"  (function),
              "b"  (*ebx),
              "c"  (*ecx),
              "d"  (*edx));

    return;
}
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Configure_CPU
 *
 * @brief    Reads the CPU information from the hardware
 *
 * @param    param   dispatch_id -  The id of the dispatch table.
 *
 * @return   Pointer to the correct dispatch table for the CPU architecture
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern  DISPATCH
UTILITY_Configure_CPU (
    U32 dispatch_id
)
{
    DISPATCH     dispatch = NULL;
    switch (dispatch_id) {
#if defined(DRV_IA32) || defined(DRV_EM64T)
        case 1:
            SEP_PRINT_DEBUG("Set up the Core(TM)2 processor dispatch table\n");
            dispatch = &core2_dispatch;
            break;
        case 6:
            SEP_PRINT_DEBUG("Set up the Silvermont dispatch table\n");
            dispatch = &silvermont_dispatch;
            break;
        case 7:
            SEP_PRINT_DEBUG("Set up the perfver4 HTON dispatch table such as Skylake\n");
            dispatch = &perfver4_dispatch;
            break;
        case 8:
            SEP_PRINT_DEBUG("Set up the perfver4 HTOFF dispatch table such as Skylake\n");
            dispatch = &perfver4_dispatch_htoff_mode;
            break;
        case 700:
            SEP_PRINT_DEBUG("Set up the Valleyview SA dispatch table\n");
            dispatch = &valleyview_visa_dispatch;
            break;
        case 710:
        case 800:
            SEP_PRINT_DEBUG("Set up the Silvermont/Haswell Server Power dispatch table\n");
            dispatch = &avoton_power_dispatch;
            break;
        case 2:
            dispatch = &corei7_dispatch;
            SEP_PRINT_DEBUG("Set up the Core i7(TM) processor dispatch table\n");
            break;
        case 3:
            SEP_PRINT_DEBUG("Set up the Core i7(TM) dispatch table\n");
            dispatch = &corei7_dispatch_htoff_mode;
            break;
        case 4:
            dispatch = &corei7_dispatch_2;
            SEP_PRINT_DEBUG("Set up the Sandybridge processor dispatch table\n");
            break;
        case 5:
            SEP_PRINT_DEBUG("Set up the Sandybridge dispatch table\n");
            dispatch = &corei7_dispatch_htoff_mode_2;
            break;
        case 9:
            dispatch = &corei7_dispatch_nehalem;
            SEP_PRINT_DEBUG("Set up the Nehalem, Westemere dispatch table\n");
            break;
        case 200:
            SEP_PRINT_DEBUG("Set up the SNB iMC dispatch table\n");
            dispatch = &snbunc_imc_dispatch;
            break;
        case 201:
            SEP_PRINT_DEBUG("Set up the SNB Cbo dispatch table\n");
            dispatch = &snbunc_cbo_dispatch;
            break;
#if !defined (DRV_ANDROID)
        case 100:
            SEP_PRINT_DEBUG("Set up the Core i7 uncore dispatch table\n");
            dispatch = &corei7_unc_dispatch;
            break;
        case 210:
            SEP_PRINT_DEBUG("Set up the WSM-EX iMC dispatch table\n");
            dispatch = &wsmexunc_imc_dispatch;
            break;
        case 211:
            SEP_PRINT_DEBUG("Set up the WSM-EX QPI dispatch table\n");
            dispatch = &wsmexunc_qpi_dispatch;
            break;
        case 212:
            SEP_PRINT_DEBUG("Set up the WSM-EX WBOX dispatch table\n");
            dispatch = &wsmexunc_wbox_dispatch;
            break;
        case 220:
            SEP_PRINT_DEBUG("Set up the JKT IMC dispatch table\n");
            dispatch = &jktunc_imc_dispatch;
            break;
        case 221:
            SEP_PRINT_DEBUG("Set up the JKT QPILL dispatch table\n");
            dispatch = &jktunc_qpill_dispatch;
            break;
        case 222:
            SEP_PRINT_DEBUG("Set up the Jaketown UBOX dispatch table\n");
            dispatch = &jaketown_ubox_dispatch;
            break;
#endif
        case 300:
            SEP_PRINT_DEBUG("Set up the SNB Power dispatch table\n");
            dispatch = &snb_power_dispatch;
            break;
        case 400:
            SEP_PRINT_DEBUG("Set up the SNB Power dispatch table\n");
            dispatch = &snbunc_gt_dispatch;
            break;
        case 500:
            SEP_PRINT_DEBUG("Set up the Haswell UNC NCU dispatch table\n");
            dispatch = &haswellunc_ncu_dispatch;
            break;
#if !defined (DRV_ANDROID)
        case 600:
            SEP_PRINT_DEBUG("Set up the IVT UNC CBO dispatch table\n");
            dispatch = &ivtunc_cbo_dispatch;
            break;
        case 610:
            SEP_PRINT_DEBUG("Set up the IVT UNC IMC dispatch table\n");
            dispatch = &ivtunc_imc_dispatch;
            break;
        case 620:
            SEP_PRINT_DEBUG("Set up the Ivytown UNC PCU dispatch table\n");
            dispatch = &ivytown_pcu_dispatch;
            break;
        case 630:
            SEP_PRINT_DEBUG("Set up the Ivytown UNC PCU dispatch table\n");
            dispatch = &ivytown_ha_dispatch;
            break;
        case 640:
            SEP_PRINT_DEBUG("Set up the Ivytown QPI dispatch table\n");
            dispatch = &ivytown_qpill_dispatch;
            break;
        case 650:
            SEP_PRINT_DEBUG("Set up the Ivytown R3QPI dispatch table\n");
            dispatch = &ivytown_r3qpi_dispatch;
            break;
        case 660:
            SEP_PRINT_DEBUG("Set up the Ivytown UNC UBOX dispatch table\n");
            dispatch = &ivytown_ubox_dispatch;
            break;
        case 670:
            SEP_PRINT_DEBUG("Set up the Ivytown UNC R2PCIe dispatch table\n");
            dispatch = &ivytown_r2pcie_dispatch;
            break;
        case 680:
            SEP_PRINT_DEBUG("Set up the Ivytown UNC IRP dispatch table\n");
            dispatch = &ivytown_irp_dispatch;
            break;
#endif
        case 720:
            SEP_PRINT_DEBUG("Set up the Haswell Power dispatch table\n");
            dispatch = &haswell_power_dispatch;
            break;
#if !defined (DRV_ANDROID)
       case 790:
            SEP_PRINT_DEBUG("Set up the Haswell Server CBO  dispatch table\n");
            dispatch = &haswell_server_cbo_dispatch;
            break;
        case 791:
            SEP_PRINT_DEBUG("Set up the Haswell Server PCU dispatch table\n");
            dispatch = &haswell_server_pcu_dispatch;
            break;
        case 792:
            SEP_PRINT_DEBUG("Set up the Haswell Server UBOX dispatch table\n");
            dispatch = &haswell_server_ubox_dispatch;
            break;
        case 793:
            SEP_PRINT_DEBUG("Set up the Haswell Server QPILL dispatch table\n");
            dispatch = &haswell_server_qpill_dispatch;
            break;
        case 794:
            SEP_PRINT_DEBUG("Set up the Haswell Server iMC dispatch table\n");
            dispatch = &haswell_server_imc_dispatch;
            break;
        case 795:
            SEP_PRINT_DEBUG("Set up the Haswell Server HA dispatch table\n");
            dispatch = &haswell_server_ha_dispatch;
            break;
        case 796:
            SEP_PRINT_DEBUG("Set up the Haswell Server R2PCIe dispatch table\n");
            dispatch = &haswell_server_r2pcie_dispatch;
            break;
        case 797:
            SEP_PRINT_DEBUG("Set up the Haswell Server R3QPI dispatch table\n");
            dispatch = &haswell_server_r3qpi_dispatch;
            break;
        case 798:
            SEP_PRINT_DEBUG("Set up the Haswell Server SBOX dispatch table\n");
            dispatch = &haswell_server_sbox_dispatch;
            break;
        case 799:
            SEP_PRINT_DEBUG("Set up the Haswell Server IRP dispatch table\n");
            dispatch = &haswell_server_irp_dispatch;
            break;
#endif
#endif
        default:
            dispatch = NULL;
            SEP_PRINT_ERROR("Architecture not supported (dispatch_id=%d)\n", dispatch_id);
            break;
    }

    return dispatch;
}

#if defined(DRV_IA32) || defined(DRV_EM64T)

extern U64
SYS_Read_MSR (
    U32   msr
)
{
    U64 val;

    rdmsrl(msr, val);

    return val;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Configure_Chipset
 *
 * @brief    Configures the chipset information
 *
 * @param    none
 *
 * @return   none
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern  CS_DISPATCH
UTILITY_Configure_Chipset (
    void
)
{
    if (CHIPSET_CONFIG_gmch_chipset(pma)) {
        cs_dispatch = &gmch_dispatch;
        SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: using GMCH dispatch table!\n");
    }
#if !defined (DRV_ANDROID)
    else if (CHIPSET_CONFIG_mch_chipset(pma) || CHIPSET_CONFIG_ich_chipset(pma)) {
        cs_dispatch = &chap_dispatch;
        SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: using CHAP dispatch table!\n");
    }
#endif
    else {
        SEP_PRINT_ERROR("UTLITY_Configure_Chipset: unable to map chipset dispatch table!\n");
    }

    SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: exiting with cs_dispatch=0x%p\n", cs_dispatch);

    return cs_dispatch;
}


#endif
