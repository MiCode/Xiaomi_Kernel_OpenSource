/* ***********************************************************************************************
  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2013 Intel Corporation. All rights reserved.

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

  Contact Information:
  SOCWatch Developer Team <socwatchdevelopers@intel.com>

  BSD LICENSE 

  Copyright(c) 2013 Intel Corporation. All rights reserved.
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

#ifndef _MATRIXIO_H_
#define _MATRIXIO_H_

#include "pw_version.h"
#include "pw_defines.h"

// #define MATRIX_IO_FILE "/dev/matrix"
#define SOCWATCH_DRIVER_NAME_ICS "socwatch"
#define SOCWATCH_DRIVER_NAME SOCWATCH_DRIVER_NAME_ICS
#define SOCWATCH_DRIVER_NAME_WITH_PATH_ICS "/dev/socwatch"
#define SOCWATCH_DRIVER_NAME_WITH_PATH SOCWATCH_DRIVER_NAME_WITH_PATH_ICS
// #define MATRIX_IO_FILE "/dev/matrix_ICS"
#define SOCWATCH_IO_FILE SOCWATCH_DRIVER_NAME_WITH_PATH_ICS

/*enumerate operations to be done in an IOCTL scan(init, poll & term) */
enum IOCtlType {
	READ_OP = 0x00000001,
	WRITE_OP = 0x00000002,
	ENABLE_OP = 0x00000004,
	SET_BITS_OP = 0x00000040,
	RESET_BITS_OP = 0x00000080,
};

#define MAX_GMCH_CTRL_REGS 4
#define MAX_GMCH_DATA_REGS 8
#define DATA_ENABLE			0x00000001
#define MTX_GMCH_PMON_GLOBAL_CTRL		0x0005F1F0
#define MTX_GMCH_PMON_GLOBAL_CTRL_ENABLE	0x0001000F
#define MTX_GMCH_PMON_GLOBAL_CTRL_DISABLE	0x00000000
#define MTX_GMCH_PMON_FIXED_CTR0		0x0005E8F0
#define MTX_GMCH_PMON_GP_CTR0_L			0x0005F8F0
#define MTX_GMCH_PMON_GP_CTR0_H			0x0005FCF0
#define MTX_GMCH_PMON_GP_CTR1_L			0x0005F9F0
#define MTX_GMCH_PMON_GP_CTR1_H			0x0005FDF0
#define MTX_GMCH_PMON_GP_CTR2_L			0x0005FAF0
#define MTX_GMCH_PMON_GP_CTR2_H			0x0005FEF0
#define MTX_GMCH_PMON_GP_CTR3_L			0x0005FBF0
#define MTX_GMCH_PMON_GP_CTR3_H			0x0005FFF0
#define MTX_GMCH_PMON_FIXED_CTR_CTRL	0x0005F4F0

#define MTX_PCI_MSG_CTRL_REG  0x000000D0
#define MTX_PCI_MSG_DATA_REG  0x000000D4

#define PWR_MGMT_BASE_ADDR_MASK      0xFFFF
#define PWR_STS_NORTH_CMPLX_LOWER    0x4
#define PWR_STS_NORTH_CMPLX_UPPER    0x30

struct mtx_msr {
	unsigned long eax_LSB;
	unsigned long edx_MSB;
	unsigned long ecx_address;
	unsigned long ebx_value;
	unsigned long n_cpu;
	unsigned long operation;
};

struct memory_map {
	unsigned long ctrl_addr;
	void *ctrl_remap_address;
	unsigned long ctrl_data;
	unsigned long data_addr;
	void *data_remap_address;
	char *ptr_data_usr;
	unsigned long data_size;
	unsigned long operation;
};

struct mtx_pci_ops {
	unsigned long port;
	unsigned long data;
	unsigned long io_type;
	unsigned long port_island;
};

struct mtx_soc_perf {
	char *ptr_data_usr;
	unsigned long data_size;
	unsigned long operation;
};

/* PCI info for a real pci device */
struct pci_config {
	unsigned long bus;
	unsigned long device;
	unsigned long function;
	unsigned long offset;
	unsigned long data; /* This is written to by the ioctl */
};
struct scu_config {
	unsigned long *address;
	unsigned char *usr_data;
	unsigned char *drv_data;
	unsigned long length;
};

struct lookup_table {
	/*Init Data */
	struct mtx_msr *msrs_init;
	unsigned long msr_init_length;
	unsigned long msr_init_wb;

	struct memory_map *mmap_init;
	unsigned long mem_init_length;
	unsigned long mem_init_wb;

	struct mtx_pci_ops *pci_ops_init;
	unsigned long pci_ops_init_length;
	unsigned long pci_ops_init_wb;

	unsigned long *cfg_db_init;
	unsigned long cfg_db_init_length;
	unsigned long cfg_db_init_wb;

	struct mtx_soc_perf *soc_perf_init;
	unsigned long soc_perf_init_length;
	unsigned long soc_perf_init_wb;

	/*Poll Data */
	struct mtx_msr *msrs_poll;
	unsigned long msr_poll_length;
	unsigned long msr_poll_wb;

	struct memory_map *mmap_poll;
	unsigned long mem_poll_length;
	unsigned long mem_poll_wb;
	unsigned long records;

	struct mtx_pci_ops *pci_ops_poll;
	unsigned long pci_ops_poll_length;
	unsigned long pci_ops_poll_wb;
	unsigned long pci_ops_records;

	unsigned long *cfg_db_poll;
	unsigned long cfg_db_poll_length;
	unsigned long cfg_db_poll_wb;

	struct scu_config scu_poll;
	unsigned long scu_poll_length;

	struct mtx_soc_perf *soc_perf_poll;
	unsigned long soc_perf_poll_length;
	unsigned long soc_perf_poll_wb;
	unsigned long soc_perf_records;

	/*Term Data */
	struct mtx_msr *msrs_term;
	unsigned long msr_term_length;
	unsigned long msr_term_wb;

	struct memory_map *mmap_term;
	unsigned long mem_term_length;
	unsigned long mem_term_wb;

	struct mtx_pci_ops *pci_ops_term;
	unsigned long pci_ops_term_length;
	unsigned long pci_ops_term_wb;

	unsigned long *cfg_db_term;
	unsigned long cfg_db_term_length;
	unsigned long cfg_db_term_wb;

	struct mtx_soc_perf *soc_perf_term;
	unsigned long soc_perf_term_length;
	unsigned long soc_perf_term_wb;
};

/*
 * 32b support in 64b kernel space
 */

#if defined (__linux__)

#ifdef __KERNEL__

#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)

#include <linux/compat.h>
// #include <asm/compat.h>

struct mtx_msr32 {
	compat_ulong_t eax_LSB;
	compat_ulong_t edx_MSB;
	compat_ulong_t ecx_address;
	compat_ulong_t ebx_value;
	compat_ulong_t n_cpu;
	compat_ulong_t operation;
};

struct memory_map32 {
	compat_ulong_t ctrl_addr;
	compat_caddr_t ctrl_remap_address;
	compat_ulong_t ctrl_data;
	compat_ulong_t data_addr;
	compat_caddr_t data_remap_address;
	compat_caddr_t ptr_data_usr;
	compat_ulong_t data_size;
	compat_ulong_t operation;
};

struct mtx_pci_ops32 {
	compat_ulong_t port;
	compat_ulong_t data;
	compat_ulong_t io_type;
	compat_ulong_t port_island;
};

struct mtx_soc_perf32 {
	compat_caddr_t ptr_data_usr;
	compat_ulong_t data_size;
	compat_ulong_t operation;
};

struct pci_config32 {
	compat_ulong_t bus;
	compat_ulong_t device;
	compat_ulong_t function;
	compat_ulong_t offset;
	compat_ulong_t data; /* This is written to by the ioctl */
};

struct scu_config32 {
	compat_caddr_t address;
	compat_caddr_t usr_data;
	compat_caddr_t drv_data;
	compat_ulong_t length;
};

struct lookup_table32 {
	/*Init Data */
	compat_caddr_t msrs_init;
	compat_ulong_t msr_init_length;
	compat_ulong_t msr_init_wb;

	compat_caddr_t mmap_init;
	compat_ulong_t mem_init_length;
	compat_ulong_t mem_init_wb;

	compat_caddr_t pci_ops_init;
	compat_ulong_t pci_ops_init_length;
	compat_ulong_t pci_ops_init_wb;

	compat_caddr_t cfg_db_init;
	compat_ulong_t cfg_db_init_length;
	compat_ulong_t cfg_db_init_wb;

	compat_caddr_t soc_perf_init;
	compat_ulong_t soc_perf_init_length;
	compat_ulong_t soc_perf_init_wb;

	/*Poll Data */
	compat_caddr_t msrs_poll;
	compat_ulong_t msr_poll_length;
	compat_ulong_t msr_poll_wb;

	compat_caddr_t mmap_poll;
	compat_ulong_t mem_poll_length;
	compat_ulong_t mem_poll_wb;
	compat_ulong_t records;

	compat_caddr_t pci_ops_poll;
	compat_ulong_t pci_ops_poll_length;
	compat_ulong_t pci_ops_poll_wb;
	compat_ulong_t pci_ops_records;

	compat_caddr_t cfg_db_poll;
	compat_ulong_t cfg_db_poll_length;
	compat_ulong_t cfg_db_poll_wb;

	struct scu_config32 scu_poll;
	compat_ulong_t scu_poll_length;

	compat_caddr_t soc_perf_poll;
	compat_ulong_t soc_perf_poll_length;
	compat_ulong_t soc_perf_poll_wb;
	compat_ulong_t soc_perf_records;

	/*Term Data */
	compat_caddr_t msrs_term;
	compat_ulong_t msr_term_length;
	compat_ulong_t msr_term_wb;

	compat_caddr_t mmap_term;
	compat_ulong_t mem_term_length;
	compat_ulong_t mem_term_wb;

	compat_caddr_t pci_ops_term;
	compat_ulong_t pci_ops_term_length;
	compat_ulong_t pci_ops_term_wb;

	compat_caddr_t cfg_db_term;
	compat_ulong_t cfg_db_term_length;
	compat_ulong_t cfg_db_term_wb;

	compat_caddr_t soc_perf_term;
	compat_ulong_t soc_perf_term_length;
	compat_ulong_t soc_perf_term_wb;
};

struct mtx_msr_container32 {
	compat_caddr_t buffer;
	compat_ulong_t length;
	struct mtx_msr32 msrType1;
};

#endif // HAVE_COMPAT_IOCTL && CONFIG_X86_64
#endif // __KERNEL__
#endif // __linux__

struct msr_buffer {
	unsigned long eax_LSB;
	unsigned long edx_MSB;
};

struct mt_msr_buffer {
	u32 eax_LSB;
	u32 edx_MSB;
};

#define MAX_SOC_PERF_VALUES 10

struct soc_perf_buffer {
	unsigned long long values[MAX_SOC_PERF_VALUES];
};

struct mt_soc_perf_buffer {
	u64 values[MAX_SOC_PERF_VALUES];
};

struct xchange_buffer {
	struct msr_buffer *ptr_msr_buff;
	unsigned long msr_length;
	unsigned long *ptr_mem_buff;
	unsigned long mem_length;
	unsigned long *ptr_pci_ops_buff;
	unsigned long pci_ops_length;
	unsigned long *ptr_cfg_db_buff;
	unsigned long cfg_db_length;
	struct soc_perf_buffer *ptr_soc_perf_buff;
	unsigned long soc_perf_length;
};

struct mt_xchange_buffer {
	u64 ptr_msr_buff;
	u64 ptr_mem_buff;
	u64 ptr_pci_ops_buff;
	u64 ptr_cfg_db_buff;
	u64 ptr_soc_perf_buff;
	u32 msr_length;
	u32 mem_length;
	u32 pci_ops_length;
	u32 cfg_db_length;
	u32 soc_perf_length;
        u32 padding;           // Required to keep sizeof(mt_xchange_buffer) the same on 32b and 64b systems 
                               // in the absence of #pragma pack(XXX) directives!
};

struct xchange_buffer_all {
	unsigned long long init_time_stamp;
	unsigned long long *poll_time_stamp;
	unsigned long long term_time_stamp;
	unsigned long long init_tsc;
	unsigned long long  term_tsc;
	unsigned long long *poll_tsc;
	struct xchange_buffer xhg_buf_init;
	struct xchange_buffer xhg_buf_poll;
	struct xchange_buffer xhg_buf_term;
	unsigned long status;
};

struct mtx_msr_container {
	unsigned long *buffer;
	unsigned long length;
	struct mtx_msr msrType1;
};

struct gmch_container {
	unsigned long long time_stamp;
	unsigned long read_mask;
	unsigned long write_mask;
	unsigned long mcr1[MAX_GMCH_CTRL_REGS];
	unsigned long mcr2[MAX_GMCH_CTRL_REGS];
	unsigned long mcr3[MAX_GMCH_CTRL_REGS];
	unsigned long data[MAX_GMCH_DATA_REGS];
	unsigned long event[MAX_GMCH_CTRL_REGS];
	unsigned long core_clks;
};

struct mtx_size_info {
	unsigned int init_msr_size;
	unsigned int term_msr_size;
	unsigned int poll_msr_size;
	unsigned int init_mem_size;
	unsigned int term_mem_size;
	unsigned int poll_mem_size;
	unsigned int init_pci_ops_size;
	unsigned int term_pci_ops_size;
	unsigned int poll_pci_ops_size;
	unsigned int init_cfg_db_size;
	unsigned int term_cfg_db_size;
	unsigned int poll_cfg_db_size;
	unsigned int poll_scu_drv_size;
	unsigned int total_mem_bytes_req;
	unsigned int init_soc_perf_size;
	unsigned int term_soc_perf_size;
	unsigned int poll_soc_perf_size;
};

#define IOCTL_INIT_SCAN _IOR(0xF8, 0x00000001, unsigned long)
#define IOCTL_TERM_SCAN _IOR(0xF8, 0x00000002, unsigned long)
#define IOCTL_POLL_SCAN _IOR(0xF8, 0x00000004, unsigned long)

#define IOCTL_INIT_MEMORY _IOR(0xF8, 0x00000010, struct xchange_buffer_all *)
#define IOCTL_FREE_MEMORY _IO(0xF8, 0x00000020)

#define IOCTL_READ_PCI_CONFIG	_IOWR(0xF8, 0x00000001, struct pci_config *)

#define IOCTL_VERSION_INFO _IOW(0xF8, 0x00000001, char *)
#define IOCTL_COPY_TO_USER _IOW(0xF8, 0x00000002, struct xchange_buffer_all *)
#define IOCTL_READ_CONFIG_DB _IOW(0xF8, 0x00000004, unsigned long *)
#define IOCTL_WRITE_CONFIG_DB _IOW(0xF8, 0x00000010, unsigned long *)
#define IOCTL_OPERATE_ON_MSR _IOW(0xF8, 0x00000020, struct mtx_msr *)

#define IOCTL_MSR _IOW(0xF8, 0x00000040, struct mtx_msr_container *)
#define IOCTL_SRAM _IOW(0xF8, 0x00000080, struct memory_map *)
#define IOCTL_GMCH_RESET _IOW(0xF8, 0x00000003, struct gmch_container *)
#define IOCTL_GMCH _IOW(0xF8, 0x00000005, struct gmch_container *)

#define IOCTL_GET_SOC_STEPPING _IOR(0xF8, 0x00000100, unsigned long *)
#define IOCTL_GET_SCU_FW_VERSION _IOR(0xF8, 0x00000200, unsigned long *)

#define IOCTL_GET_DRIVER_VERSION _IOW(0xF8, 0x00000400, unsigned long *)

#if defined (__linux__)

#ifdef __KERNEL__

#if defined(HAVE_COMPAT_IOCTL) && defined(CONFIG_X86_64)
    #define IOCTL_INIT_SCAN32 _IOR(0xF8, 0x00000001, compat_ulong_t)
    #define IOCTL_TERM_SCAN32 _IOR(0xF8, 0x00000002, compat_ulong_t)
    #define IOCTL_POLL_SCAN32 _IOR(0xF8, 0x00000004, compat_ulong_t)

    #define IOCTL_INIT_MEMORY32 _IOR(0xF8, 0x00000010, compat_uptr_t)
    #define IOCTL_FREE_MEMORY32 _IO(0xF8, 0x00000020)

    #define IOCTL_READ_PCI_CONFIG32	_IOWR(0xF8, 0x00000001, compat_uptr_t)

    #define IOCTL_VERSION_INFO32 _IOW(0xF8, 0x00000001, compat_caddr_t)
    #define IOCTL_COPY_TO_USER32 _IOW(0xF8, 0x00000002, compat_uptr_t)
    #define IOCTL_READ_CONFIG_DB32 _IOW(0xF8, 0x00000004, compat_uptr_t)
    #define IOCTL_WRITE_CONFIG_DB32 _IOW(0xF8, 0x00000010, compat_uptr_t)
    #define IOCTL_OPERATE_ON_MSR32 _IOW(0xF8, 0x00000020, compat_uptr_t)

    #define IOCTL_MSR32 _IOW(0xF8, 0x00000040, compat_uptr_t)
    #define IOCTL_SRAM32 _IOW(0xF8, 0x00000080, compat_uptr_t)
    #define IOCTL_GMCH_RESET32 _IOW(0xF8, 0x00000003, compat_uptr_t)
    #define IOCTL_GMCH32 _IOW(0xF8, 0x00000005, compat_uptr_t)

    #define IOCTL_GET_SOC_STEPPING32 _IOR(0xF8, 0x00000100, compat_uptr_t)
    #define IOCTL_GET_SCU_FW_VERSION32 _IOR(0xF8, 0x00000200, compat_uptr_t)

    #define IOCTL_GET_DRIVER_VERSION32 _IOW(0xF8, 0x00000400, compat_uptr_t)
#endif // HAVE_COMPAT_IOCTL && CONFIG_X86_64
#endif // __KERNEL__
#endif // __linux__

#define platform_pci_read32	intel_mid_msgbus_read32_raw
#define platform_pci_write32	intel_mid_msgbus_write32_raw

#endif
