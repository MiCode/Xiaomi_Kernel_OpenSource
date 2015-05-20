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
#include <linux/errno.h>
#include <linux/types.h>
#include <asm/page.h>
#include <asm/io.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"

#include "lwpmudrv_chipset.h"

#include "lwpmudrv.h"
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

    SEP_PRINT_DEBUG("PCI_Read_From_Memory_Address: reading physcial address:%x\n",addr);
    offset       = addr & ~PAGE_MASK;
    aligned_addr = addr & PAGE_MASK;
    SEP_PRINT_DEBUG("PCI_Read_From_Memory_Address: aligned physcial address:%x,offset:%x\n",aligned_addr,offset);

    base = ioremap_nocache(aligned_addr, PAGE_SIZE);
    if (base == NULL) {
        return OS_INVALID;
    }

    value = readl(base+offset);
    *val = value;
    SEP_PRINT_DEBUG("PCI_Read_From_Memory_Address: value at this physical address:%x\n",value);

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

    SEP_PRINT_DEBUG("PCI_Write_To_Memory_Address: writing physcial address:%x with value:%x\n",addr,val);
    offset       = addr & ~PAGE_MASK;
    aligned_addr = addr & PAGE_MASK;
    SEP_PRINT_DEBUG("PCI_Write_To_Memory_Address: aligned physcial address:%x,offset:%x\n",aligned_addr,offset);

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
