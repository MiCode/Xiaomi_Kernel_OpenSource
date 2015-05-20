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


#ifndef _SOC_UNCORE_H_INC_
#define _SOC_UNCORE_H_INC_

/*
 * Local to this architecture: SoC uncore unit 
 * 
 */
#define SOC_UNCORE_DESKTOP_DID                 0x000C04
#define SOC_UNCORE_NEXT_ADDR_OFFSET            4
#define SOC_UNCORE_BAR_ADDR_SHIFT              32
#define SOC_UNCORE_BAR_ADDR_MASK               0x000FFFC00000LL
#define SOC_UNCORE_MAX_PCI_DEVICES             16
#define SOC_UNCORE_MCR_REG_OFFSET              0xD0
#define SOC_UNCORE_MDR_REG_OFFSET              0xD4
#define SOC_UNCORE_MCRX_REG_OFFSET             0xD8
#define SOC_UNCORE_BYTE_ENABLES                0xF
#define SOC_UNCORE_OP_CODE_SHIFT               24
#define SOC_UNCORE_PORT_ID_SHIFT               16
#define SOC_UNCORE_OFFSET_HI_MASK              0xFFFFFF00
#define SOC_UNCORE_OFFSET_LO_MASK              0xFF
#define SOC_COUNTER_PORT_ID                    23
#define SOC_COUNTER_WRITE_OP_CODE              1
#define SOC_COUNTER_READ_OP_CODE               0
#define UNCORE_MAX_COUNTERS                    8
#define UNCORE_MAX_COUNT                       0x00000000FFFFFFFFLL

#define SOC_UNCORE_OTHER_BAR_MMIO_PAGE_SIZE    4096
#define SOC_UNCORE_SAMPLE_DATA                 0x00020000
#define SOC_UNCORE_STOP                        0x00040000
#define SOC_UNCORE_CTRL_REG_OFFSET             0x0


extern DISPATCH_NODE  soc_uncore_dispatch;

#endif
