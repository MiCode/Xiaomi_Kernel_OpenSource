/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.

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

  Copyright(c) 2005-2014 Intel Corporation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/types.h>
#include <asm/page.h>
#include <asm/io.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "socperfdrv.h"
#include "pci.h"

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern int PCI_Read_From_Memory_Address(addr, val)
 *
 * @param    addr    - physical address in mmio
 * @param   *value  - value at this address
 *
 * @return  status
 *
 * @brief   Read memory mapped i/o physical location
 *
 */
extern int
PCI_Read_From_Memory_Address (
    U32 addr,
    U32* val
)
{
    U32 aligned_addr, offset, value;
    PVOID base;

    if (addr <= 0) {
        return OS_INVALID;
    }

    SOCPERF_PRINT_DEBUG("PCI_Read_From_Memory_Address: reading physical address:%x\n",addr);
    offset       = addr & ~PAGE_MASK;
    aligned_addr = addr & PAGE_MASK;
    SOCPERF_PRINT_DEBUG("PCI_Read_From_Memory_Address: aligned physical address:%x,offset:%x\n",aligned_addr,offset);

    base = ioremap_nocache(aligned_addr, PAGE_SIZE);
    if (base == NULL) {
        return OS_INVALID;
    }

    value = readl(base+offset);
    *val = value;
    SOCPERF_PRINT_DEBUG("PCI_Read_From_Memory_Address: value at this physical address:%x\n",value);

    iounmap(base);

    return OS_SUCCESS;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn extern int PCI_Write_To_Memory_Address(addr, val)
 *
 * @param   addr   - physical address in mmio
 * @param   value  - value to be written
 *
 * @return  status
 *
 * @brief   Write to memory mapped i/o physical location
 *
 */
extern int
PCI_Write_To_Memory_Address (
    U32 addr,
    U32 val
)
{
    U32 aligned_addr, offset;
    PVOID base;

    if (addr <= 0) {
        return OS_INVALID;
    }

    SOCPERF_PRINT_DEBUG("PCI_Write_To_Memory_Address: writing physical address:%x with value:%x\n",addr,val);
    offset       = addr & ~PAGE_MASK;
    aligned_addr = addr & PAGE_MASK;
    SOCPERF_PRINT_DEBUG("PCI_Write_To_Memory_Address: aligned physical address:%x,offset:%x\n",aligned_addr,offset);

    base = ioremap_nocache(aligned_addr, PAGE_SIZE);
    if (base == NULL) {
        return OS_INVALID;
    }

    writel(val,base+offset);

    iounmap(base);

    return OS_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern int PCI_Read_Ulong(pci_address)
 *
 * @param    pci_address - PCI configuration address
 *
 * @return  value at this location
 *
 * @brief   Reads a ULONG from PCI configuration space
 *
 */
extern int
PCI_Read_Ulong (
    U32 pci_address
)
{
    U32 temp_ulong = 0;

    outl(pci_address,PCI_ADDR_IO);
    temp_ulong = inl(PCI_DATA_IO);

    return temp_ulong;
}


/* ------------------------------------------------------------------------- */
/*!
 * @fn extern int PCI_Write_Ulong(addr, val)
 *
 * @param    pci_address - PCI configuration address
 * @param    value - Value to be written
 *
 * @return  status
 *
 * @brief   Writes a ULONG to PCI configuration space
 *
 */
extern void
PCI_Write_Ulong (
    U32 pci_address,
    U32 value
)
{
    outl(pci_address, PCI_ADDR_IO);
    outl(value, PCI_DATA_IO);

    return;
}
