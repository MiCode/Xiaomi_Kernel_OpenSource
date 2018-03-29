/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include "mtk_mali_kernel.h"
#include "mali_kernel_common.h" /*for mali printf*/
/*#include <mach/mt_clkmgr.h>  */   /*For MFG sub-system clock control API*/
/*#include <linux/earlysuspend.h>*/ /*For early suspend*/
/*#include <mach/mt_clkmgr.h>*/

void MTKMALI_DumpRegister( void )
{
#if 0
#define DUMP_REG_INFO( addr )   MALIK_MSG("REG: %s = 0x%08x\n", #addr, M_READ32( addr, 0 ))
    unsigned long dummy;

    MALIK_MSG("MTKMALI_DumpRegister-------:\n"); 
    MALIK_MSG("MT_CG_MFG_G3D is %d\n", clock_is_on(MT_CG_MFG_G3D));
    MALIK_MSG("MT_CG_DISP0_SMI_COMMON is %d\n", clock_is_on(MT_CG_DISP0_SMI_COMMON));

    /*Dump Clock Gating Register*/
    DUMP_REG_INFO( REG_SMI_CG_TEMP );
    DUMP_REG_INFO( REG_MFG_CG_CON );
    DUMP_REG_INFO( REG_MFG_RESET );
    DUMP_REG_INFO( REG_MFG_DEBUG_SEL );

    /*Test Mali Register*/
    dummy = ( 0x1F-M_READ32( REG_MFG_DEBUG_SEL, 0x0 ) );
    MALIK_MSG("Write 0x%02X  to  REG_MFG_DEBUG_SEL\n", (unsigned int)dummy );
    M_WRITE32( REG_MFG_DEBUG_SEL, 0x0, dummy );
    DUMP_REG_INFO( REG_MFG_DEBUG_SEL );

    MALIK_MSG("---------------------------:\n"); 


   /*Dump Call stack*/
    dump_stack();
#endif // 0  
}

