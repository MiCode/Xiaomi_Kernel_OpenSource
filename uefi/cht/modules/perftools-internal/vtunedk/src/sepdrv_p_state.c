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
#include <linux/version.h>
#include <linux/errno.h>
#include "lwpmudrv_defines.h"
#include "lwpmudrv_types.h"
#include "control.h"
#include "utility.h"
#include "sepdrv_p_state.h"

/*!
 * @fn     OS_STATUS SEPDRV_P_STATE_Read
 *
 * @brief  Reads the APERF and MPERF counters into the buffer provided for the purpose
 *
 * @param  buffer  - buffer to read the counts into
 *
 * @param  pcpu - pcpu struct that contains the previous APERF/MPERF values
 *
 * @return OS_SUCCESS if read succeeded, otherwise error
 *
 * @note
 */
#if defined(DRV_IA32) || defined(DRV_EM64T)
extern OS_STATUS
SEPDRV_P_STATE_Read (
    S8 *buffer,
    CPU_STATE pcpu
)
{
    U64  *samp  = (U64 *)buffer;
    U64  new_APERF = 0;
    U64  new_MPERF = 0;

    if ((samp == NULL) || (pcpu == NULL)) {
        return OS_INVALID;
    }

    new_APERF = SYS_Read_MSR(DRV_APERF_MSR);
    new_MPERF = SYS_Read_MSR(DRV_MPERF_MSR);

    if (CPU_STATE_last_p_state_valid(pcpu)) {
        // there is a previous APERF/MPERF value
        if ((CPU_STATE_last_aperf(pcpu)) > new_APERF) {
            // a wrap-around has occurred.
            samp[1] = CPU_STATE_last_aperf(pcpu) - new_APERF;
        }
        else {
            samp[1] = new_APERF - CPU_STATE_last_aperf(pcpu);
        }

        if ((CPU_STATE_last_mperf(pcpu)) > new_MPERF) {
            // a wrap-around has occurred.
            samp[0] = CPU_STATE_last_mperf(pcpu) - new_MPERF;
        }
        else {
            samp[0] = new_MPERF - CPU_STATE_last_mperf(pcpu);
        }
    }
    else {
        // there is no previous valid APERF/MPERF values, thus no delta calculations
        (CPU_STATE_last_p_state_valid(pcpu)) = TRUE;
        samp[0] = 0;
        samp[1] = 0;
    }

    CPU_STATE_last_aperf(pcpu) = new_APERF;
    CPU_STATE_last_mperf(pcpu) = new_MPERF;

    return OS_SUCCESS;
}
#endif
