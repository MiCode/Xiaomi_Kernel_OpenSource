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

#ifndef _ECB_ITERATORS_H_
#define _ECB_ITERATORS_H_

#if defined(__cplusplus)
extern "C" {
#endif

//
// Loop macros to walk through the event control block
// Use for access only in the kernel mode
// To Do - Control access from kernel mode by a macro
//

#define FOR_EACH_PCI_DATA_REG(pecb,i, device_idx, offset_delta) {                                       \
    U32                 (i)    = 0;                                                                     \
    U32              (cur_grp) = LWPMU_DEVICE_cur_group(device_uncore);                                   \
    ECB                 (pecb) = LWPMU_DEVICE_PMU_register_data(device_uncore[(cur_grp)];                 \
    if ((pecb)) {                                                                                       \
        for ((i) = ECB_data_start(pecb);                                                                \
             (i) < ECB_data_start(pecb)+ECB_data_pop(pecb);                                             \
             (i)++) {                                                                                   \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                           \
                continue;                                                                               \
            }                                                                                           \
            (offset_delta) =  ECB_entries_pci_id_offset(pecb,i) -                                       \
                              DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(&ECB_pcidev_entry_node(pecb));

#define END_FOR_EACH_PCI_DATA_REG    } } }

#define FOR_EACH_PCI_DATA_REG_RAW(pecb,i, device_idx ) {                                                \
    U32                 (i)       = 0;                                                                  \
    U32                 (cur_grp) = LWPMU_DEVICE_cur_group(device_uncore);                                \
    ECB                 (pecb)    = LWPMU_DEVICE_PMU_register_data(device_uncore)[(cur_grp)];             \
    if ((pecb)) {                                                                                       \
        for ((i) = ECB_data_start(pecb);                                                                \
             (i) < ECB_data_start(pecb)+ECB_data_pop(pecb);                                             \
             (i)++) {                                                                                   \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                           \
                continue;                                                                               \
            }

#define END_FOR_EACH_PCI_DATA_REG_RAW    } } }

#define FOR_EACH_PCI_CCCR_REG_RAW(pecb,i, device_idx ) {                                            \
    U32              (i)       = 0;                                                                 \
    U32              (cur_grp) = LWPMU_DEVICE_cur_group(device_uncore);                               \
    ECB              (pecb)    = LWPMU_DEVICE_PMU_register_data(device_uncore)[(cur_grp)];            \
    if ((pecb)) {                                                                                   \
        for ((i) = ECB_cccr_start(pecb);                                                            \
             (i) < ECB_cccr_start(pecb)+ECB_cccr_pop(pecb);                                         \
             (i)++) {                                                                               \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                       \
                continue;                                                                           \
            }

#define END_FOR_EACH_PCI_CCCR_REG_RAW   } } }

#define FOR_EACH_PCI_REG_RAW(pecb, i, device_idx ) {                                                   \
    U32                 (i)       = 0;                                                                 \
    U32                 (cur_grp) = LWPMU_DEVICE_cur_group(device_uncore);                       \
    ECB                 (pecb)    = LWPMU_DEVICE_PMU_register_data(device_uncore)[(cur_grp)]; \
    if ((pecb)) {                                                                                      \
        for ((i) = 0;                                                                                  \
             (i) < ECB_num_entries(pecb);                                                              \
             (i)++) {                                                                                  \
            if (ECB_entries_pci_id_offset((pecb),(i)) == 0) {                                          \
                continue;                                                                              \
            }

#define END_FOR_EACH_PCI_REG_RAW   } } }


#if defined(__cplusplus)
}
#endif

#endif
