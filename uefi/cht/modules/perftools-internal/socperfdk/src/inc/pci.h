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

#ifndef _PCI_H_
#define _PCI_H_

#include "lwpmudrv_defines.h"

/*
 * PCI Config Address macros
 */
#define PCI_ENABLE                          0x80000000

#define PCI_ADDR_IO                         0xCF8
#define PCI_DATA_IO                         0xCFC

#define BIT0                                0x1
#define BIT1                                0x2

/*
 * Macro for forming a PCI configuration address
 */
#define FORM_PCI_ADDR(bus,dev,fun,off)     (((PCI_ENABLE))          |   \
                                            ((bus & 0xFF) << 16)    |   \
                                            ((dev & 0x1F) << 11)    |   \
                                            ((fun & 0x07) <<  8)    |   \
                                            ((off & 0xFF) <<  0))

#define VENDOR_ID_MASK                        0x0000FFFF
#define DEVICE_ID_MASK                        0xFFFF0000
#define DEVICE_ID_BITSHIFT                    16
#define LOWER_4_BYTES_MASK                    0x00000000FFFFFFFF
#define MAX_BUSNO                             256
#define NEXT_ADDR_OFFSET                      4
#define NEXT_ADDR_SHIFT                       32
#define DRV_IS_PCI_VENDOR_ID_INTEL            0x8086

#define CHECK_IF_GENUINE_INTEL_DEVICE(value, vendor_id, device_id)    \
    {                                                                 \
        vendor_id = value & VENDOR_ID_MASK;                           \
        device_id = (value & DEVICE_ID_MASK) >> DEVICE_ID_BITSHIFT;   \
                                                                      \
        if (vendor_id != DRV_IS_PCI_VENDOR_ID_INTEL) {                \
            continue;                                                 \
        }                                                             \
                                                                      \
    }

#if defined(DRV_IA32) || defined(DRV_EM64T)
extern int
PCI_Read_From_Memory_Address (
    U32 addr,
    U32* val
);

extern int
PCI_Write_To_Memory_Address (
    U32 addr,
    U32 val
);

extern int
PCI_Read_Ulong (
    U32 pci_address
);

extern void
PCI_Write_Ulong (
    U32 pci_address,
    U32 value
);
#endif

#endif  
